"""Recover original Dorado textures into a deterministic Unreal-ready PNG package.

Source DDS files are read only; the external unsplitter only sees workspace copies.
DDNA is tangent normal RGB plus gloss alpha. CryEngine and Unreal use DirectX Y-
normals. BC5 decoders provide RG only: reconstruct positive Z before PNG export.
Gloss is converted to perceptual roughness by 1 - gloss. No gold emission is added.
"""
import argparse
import concurrent.futures
import hashlib
import io
import json
import re
import shutil
import struct
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageOps


SLOTS = {'Diffuse': 'BaseColor', 'Bumpmap': 'Normal', 'Specular': 'SpecularColor', 'Emittance': 'Emissive', 'Opacity': 'BlendMask'}
NONMETAL_SHADERS = {'water', 'waterfall', 'vegetation', 'sky', 'cloud', 'clouds', 'geometrybeam', 'beam', 'mist', 'fog', 'fogvolume', 'volumetricfog'}


def decode_dds(path):
    data = bytearray(path.read_bytes())
    # BC1/2/3 sRGB and UNORM encode identical blocks. Pillow does not dispatch
    # sRGB enums 72/75/78, so remap only an in-memory header, preserving encoded
    # color values. The material manifest carries the sRGB interpretation.
    if data[84:88] == b'DX10':
        dxgi = struct.unpack_from('<I', data, 128)[0]
        if dxgi in (72, 75, 78):
            struct.pack_into('<I', data, 128, dxgi-1)
    image = Image.open(io.BytesIO(data))
    image.load()
    return image


def recover_available_mip(local):
    """Recover a valid lower mip if the shipped source lacks its largest mip."""
    parts = sorted((p for p in local.parent.glob(local.name+'.*') if p.name[len(local.name)+1:].isdigit()), key=lambda p: int(p.name[len(local.name)+1:]))
    parts = [p for p in parts if p.suffix != '.0']
    if not parts:
        raise RuntimeError('No independently recoverable source mip.')
    largest = parts[-1]
    header = bytearray(local.read_bytes()[:148])
    width, height = struct.unpack_from('<I', header, 16)[0], struct.unpack_from('<I', header, 12)[0]
    dxgi = struct.unpack_from('<I', header, 128)[0]
    block = 8 if dxgi in (70, 71, 72, 79, 80, 81) else 16
    payload = largest.read_bytes()
    while max(1, (width+3)//4)*max(1, (height+3)//4)*block > len(payload):
        width, height = max(1, width//2), max(1, height//2)
    if max(1, (width+3)//4)*max(1, (height+3)//4)*block != len(payload):
        raise RuntimeError('Available mip size does not match DDS compression geometry.')
    struct.pack_into('<I', header, 12, height)
    struct.pack_into('<I', header, 16, width)
    struct.pack_into('<I', header, 20, len(payload))
    struct.pack_into('<I', header, 28, 1)
    recovered = local.with_name(local.stem+'.available_mip.dds')
    recovered.write_bytes(header+payload)
    gloss = None
    alpha = largest.with_name(largest.name+'a')
    if alpha.exists() and alpha.stat().st_size == max(1, (width+3)//4)*max(1, (height+3)//4)*8:
        struct.pack_into('<I', header, 128, 80)
        struct.pack_into('<I', header, 20, alpha.stat().st_size)
        gloss = local.with_name(local.stem+'.available_mip_gloss.dds')
        gloss.write_bytes(header+alpha.read_bytes())
    return recovered, gloss, (width, height)


def clean(value):
    return re.sub('[^a-z0-9_]+', '_', value.lower()).strip('_')


def source_key(value):
    return str(Path(value.replace('\\', '/')).with_suffix('.dds')).replace('\\', '/').lower()


def rgba(value, default=(1, 1, 1, 1)):
    if not value:
        return list(default)
    values = [float(x) for x in value.split(',')]
    return values + list(default[len(values):])


def update_material_metadata(material, props):
    material['emission_color_source'] = rgba(props.get('Emittance'), (0, 0, 0, 0))[:3]
    material['emission_strength_source'] = rgba(props.get('Emittance'), (0, 0, 0, 0))[3]
    material['emissive_color_source'] = rgba(props.get('Emissive'), (0, 0, 0, 0))[:3]
    material['emission_source_fields'] = {'Emittance': props.get('Emittance'), 'Emissive': props.get('Emissive')}
    special = material['shader'].lower() in NONMETAL_SHADERS or any(word in material['name'].lower() for word in ('cloud', 'mist', 'fogplane'))
    if special:
        material['metallic'] = 0.0
        material['pbr_conversion_excluded'] = True
        material['maps'].pop('Metallic', None)
        if 'OriginalDiffuse' in material['maps']:
            material['maps']['BaseColor'] = dict(material['maps']['OriginalDiffuse'])
            material['color_factor'] = material.get('source_color_factor', material['color_factor'])
        material['notes'] = [n for n in material['notes'] if not n.startswith('BaseColor/Metallic derived') and not n.startswith('Metallic scalar inferred')]
        note = 'Energy/cloud/mist/water/foliage shader: diffuse retained and metallic disabled; opacity requires source-shader interpretation.'
        if note not in material['notes']:
            material['notes'].append(note)


def srgb_to_linear(rgb):
    return np.where(rgb <= .04045, rgb / 12.92, ((rgb+.055)/1.055)**2.4)


def linear_to_srgb(rgb):
    rgb = np.clip(rgb, 0, 1)
    return np.where(rgb <= .0031308, rgb*12.92, 1.055*rgb**(1/2.4)-.055)


def convert_specular_workflow(material, source_props, output):
    """Vectorized energy-preserving approximation of the Khronos conversion.

    The original shader is not reproduced exactly. Retain original diffuse and
    specular PNGs so reviewers can change this conversion without source loss.
    """
    maps = material['maps']
    if material.get('pbr_conversion_excluded') or material['shader'].lower() in NONMETAL_SHADERS:
        material['metallic'] = 0.0
        material['notes'].append('Special source shader: preserve diffuse; use dedicated nonmetallic native shader.')
        return
    if 'BaseColor' not in maps or 'SpecularColor' not in maps:
        return
    if maps['BaseColor']['uv_scale'] != maps['SpecularColor']['uv_scale']:
        material['notes'].append('Specular workflow conversion skipped: diffuse/specular UV transforms differ.')
        return
    diffuse_im = Image.open(maps['BaseColor']['path']).convert('RGB')
    spec_im = Image.open(maps['SpecularColor']['path']).convert('RGB').resize(diffuse_im.size, Image.Resampling.BILINEAR)
    diffuse = srgb_to_linear(np.asarray(diffuse_im, dtype=np.float32)/255) * np.asarray(material['color_factor'][:3])
    specular = srgb_to_linear(np.asarray(spec_im, dtype=np.float32)/255) * np.asarray(rgba(source_props.get('Specular'))[:3])
    diffuse = np.clip(diffuse, 0, 1)
    specular = np.clip(specular, 0, 1)
    weights = np.asarray([.299, .587, .114])
    d_bright = np.sqrt(np.sum(diffuse*diffuse*weights, axis=-1))
    s_bright = np.sqrt(np.sum(specular*specular*weights, axis=-1))
    remaining = 1-np.max(specular, axis=-1)
    middle = d_bright*remaining/.96+s_bright-.08
    discriminant = np.maximum(middle*middle-.16*(.04-s_bright), 0)
    metallic = np.clip((-middle+np.sqrt(discriminant))/.08, 0, 1)
    metallic[s_bright < .04] = 0
    m = metallic[:, :, None]
    from_diffuse = diffuse*remaining[:, :, None]/.96/np.maximum(1-m, .000001)
    from_specular = (specular-.04*(1-m))/np.maximum(m, .000001)
    base = np.clip(from_diffuse*(1-m*m)+from_specular*m*m, 0, 1)
    pbr = output/'PreparedPBR'
    pbr.mkdir(parents=True, exist_ok=True)
    # Include a hash to keep names stable while limiting Windows path length.
    material_token = clean(material['name'])[:42]+'_'+hashlib.sha1(material['id'].encode()).hexdigest()[:8]
    base_path, metal_path = pbr/(material_token+'_BC.png'), pbr/(material_token+'_M.png')
    Image.fromarray(np.uint8(linear_to_srgb(base)*255+.5)).save(base_path)
    Image.fromarray(np.uint8(metallic*255+.5)).save(metal_path)
    original_base = dict(maps['BaseColor'])
    maps['OriginalDiffuse'] = original_base
    maps['BaseColor'] = {**original_base, 'path': str(base_path.resolve()), 'conversion': 'SpecularGlossinessToMetallicRoughness'}
    maps['Metallic'] = {**original_base, 'path': str(metal_path.resolve()), 'srgb': False, 'conversion': 'SpecularGlossinessToMetallicRoughness'}
    material['source_color_factor'] = material['color_factor']
    material['color_factor'] = [1, 1, 1, 1]
    material['metallic'] = 1.0
    material['notes'].append('BaseColor/Metallic derived from original diffuse/specular with source linear factors; factor is baked, do not apply twice. Metal value is a map multiplier.')


def prepare_one(item, output, converter, max_dimension):
    key, record = item
    source = Path(record['source'])
    digest = hashlib.sha256()
    parts = [source] + sorted(p for p in source.parent.glob(source.name+'.*') if re.fullmatch(r'(?:[0-9]+a?|a)', p.name[len(source.name)+1:]))
    for part in parts:
        digest.update(part.name.encode())
        digest.update(part.read_bytes())
    token = hashlib.sha1(key.encode()).hexdigest()[:8]
    asset_id = clean(source.stem) + '_' + token
    staging = output/'RecoveredDDS'/asset_id
    staging.mkdir(parents=True, exist_ok=True)
    for part in parts:
        dest = staging/part.name
        if not dest.exists() or dest.read_bytes() != part.read_bytes():
            shutil.copyfile(part, dest)
    local = staging/source.name
    combined = local.with_name(local.stem+'.combined.dds')
    gloss_path = local.with_name(local.stem+'.combined_gloss.dds')
    combined_cache = staging/'recovery_hash.txt'
    if len(parts) > 1:
        if not combined.exists() or not combined_cache.exists() or combined_cache.read_text() != digest.hexdigest():
            result = subprocess.run([str(converter), str(local), '--safe'], capture_output=True, text=True, timeout=60)
            if result.returncode != 0 or not combined.exists():
                raise RuntimeError('DDS recovery failed: '+result.stdout[-1200:]+result.stderr[-1200:])
            combined_cache.write_text(digest.hexdigest())
    else:
        combined = local
    notes = []
    try:
        image = decode_dds(combined)
    except OSError:
        combined, fallback_gloss, dimensions = recover_available_mip(local)
        if fallback_gloss:
            gloss_path = fallback_gloss
        image = decode_dds(combined)
        notes.append('Original highest mip missing; recovered largest available source mip '+str(dimensions)+'. No pixels fabricated.')
    original_size = image.size
    png_dir = output/'Prepared'
    png_dir.mkdir(parents=True, exist_ok=True)
    outputs = {}
    roles = record['roles']
    if max(image.size) > max_dimension:
        image.thumbnail((max_dimension, max_dimension), Image.Resampling.LANCZOS)
        notes.append('Prepared image downsampled; full-resolution recovered DDS preserved.')

    def save(im, suffix):
        path = png_dir/(asset_id+'_'+suffix+'.png')
        im.save(path, compress_level=6)
        return str(path.resolve())

    if 'Normal' in roles:
        pixels = np.asarray(image.convert('RGB'), dtype=np.float32) / 255.0
        header = combined.read_bytes()[:148]
        dxgi = struct.unpack_from('<I', header, 128)[0] if header[84:88] == b'DX10' else None
        bc5 = dxgi in (82, 83, 84) or header[84:88] in (b'ATI2', b'BC5U', b'BC5S')
        nx = pixels[:, :, 0]*2-1
        ny = pixels[:, :, 1]*2-1
        if bc5 or np.std(pixels[:, :, 2]) < .002:
            nz = np.sqrt(np.maximum(0, 1-nx*nx-ny*ny))
            notes.append('Positive tangent Z reconstructed from BC5 RG; green channel preserved (DirectX Y-).')
        else:
            nz = pixels[:, :, 2]*2-1
        normal = np.stack((nx, ny, nz), axis=-1)
        normal /= np.maximum(np.linalg.norm(normal, axis=-1, keepdims=True), .00001)
        outputs['Normal'] = save(Image.fromarray(np.uint8(np.clip(normal*.5+.5, 0, 1)*255)), 'N')
        if gloss_path.exists():
            gloss = decode_dds(gloss_path).convert('L').resize(image.size, Image.Resampling.BILINEAR)
            outputs['Gloss'] = save(gloss, 'Gloss')
            outputs['Roughness'] = save(ImageOps.invert(gloss), 'R')
        elif '_ddna' in source.stem.lower() and image.mode == 'RGBA':
            gloss = image.getchannel('A')
            outputs['Gloss'] = save(gloss, 'Gloss')
            outputs['Roughness'] = save(ImageOps.invert(gloss), 'R')
        else:
            notes.append('No gloss alpha recovered; material roughness scalar required.')
    for role, suffix in [('BaseColor', 'BC'), ('SpecularColor', 'Spec'), ('Emissive', 'E'), ('BlendMask', 'Mask')]:
        if role in roles:
            outputs[role] = save(image.convert('RGBA') if role == 'BaseColor' else image.convert('RGB'), suffix)
    if image.mode == 'RGBA' and ('BaseColor' in roles or 'Emissive' in roles):
        alpha = image.getchannel('A')
        if alpha.getextrema() != (255, 255):
            outputs['Opacity'] = save(alpha, 'A')
    return {'source_key': key, 'source': str(source), 'source_resolution': record.get('resolution', 'exact_original_relative_path'), 'source_sha256': digest.hexdigest(), 'id': asset_id, 'original_dimensions': original_size, 'prepared_dimensions': image.size, 'recovered_dds': str(combined.resolve()), 'outputs': outputs, 'notes': notes}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mapping', default='Saved/Logs/dorado-reconstruction-20261030/source-material-mapping.json')
    parser.add_argument('--output', default='SourceArt/Dorado/Textures')
    parser.add_argument('--converter', default='F:/CryEngineConversionTools/DDS-Unsplitter.exe')
    parser.add_argument('--max-dimension', type=int, default=2048)
    parser.add_argument('--workers', type=int, default=4)
    parser.add_argument('--filter', default='')
    parser.add_argument('--metadata-only', action='store_true')
    args = parser.parse_args()
    mapping = json.loads(Path(args.mapping).read_text(encoding='utf-8'))
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    if args.metadata_only:
        manifest_path = output/'texture_manifest.json'
        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        sources = {clean(Path(m['file']).stem+'__'+str(m.get('submaterial_index', 0))+'__'+m['name']): m['properties'] for m in mapping['materials']}
        for material in manifest['materials']:
            props = sources.get(material['id'])
            if props is not None:
                update_material_metadata(material, props)
        manifest['summary']['materials_with_pbr_conversion'] = sum('Metallic' in m['maps'] for m in manifest['materials'])
        manifest_path.write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        print(json.dumps({'metadata_updated': len(manifest['materials']), 'textures_regenerated': False, 'summary': manifest['summary']}))
        return
    wanted = {}
    for material in mapping['materials']:
        if args.filter and not re.search(args.filter, material['name']+' '+material['file'], re.I):
            continue
        for tex in material['textures']:
            role = SLOTS.get(tex['slot'])
            if role and tex['resolved']:
                key = source_key(tex['original'])
                record = wanted.setdefault(key, {'source': tex['resolved'][0]['path'], 'roles': set(), 'resolution': tex.get('resolution', 'exact_original_relative_path')})
                record['roles'].add(role)
    textures = []
    errors = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        jobs = {pool.submit(prepare_one, item, output, Path(args.converter), args.max_dimension): item[0] for item in sorted(wanted.items())}
        for future in concurrent.futures.as_completed(jobs):
            key = jobs[future]
            try:
                textures.append(future.result())
            except Exception as exc:
                errors.append({'source_key': key, 'error': str(exc)})
            if (len(textures)+len(errors)) % 20 == 0:
                print(json.dumps({'prepared': len(textures), 'errors': len(errors), 'total': len(jobs)}), flush=True)
    textures.sort(key=lambda row: row['source_key'])
    by_key = {row['source_key']: row for row in textures}
    materials = []
    for material in mapping['materials']:
        if args.filter and not re.search(args.filter, material['name']+' '+material['file'], re.I):
            continue
        props = material['properties']
        index = material.get('submaterial_index', 0)
        row = {'id': clean(Path(material['file']).stem+'__'+str(index)+'__'+material['name']), 'name': material['name'], 'source_mtl': material['file'], 'submaterial_index': index, 'shader': props.get('Shader', ''), 'surface_type': props.get('SurfaceType', ''), 'maps': {}, 'color_factor': rgba(props.get('Diffuse')), 'metallic': 1.0 if ('metal' in props.get('SurfaceType', '').lower() or re.search(r'(^|_)gold($|_)', material['name'], re.I)) else 0.0, 'opacity': float(props.get('Opacity', 1)), 'roughness_default': .55, 'emission_strength_source': rgba(props.get('Emittance'), (0, 0, 0, 0))[3], 'notes': []}
        for tex in material['textures']:
            role = SLOTS.get(tex['slot'])
            key = source_key(tex['original'])
            converted = by_key.get(key)
            if not role:
                continue
            if converted and role in converted['outputs']:
                uv = [float(tex['texMod'].get('TileU', 1)), float(tex['texMod'].get('TileV', 1))]
                row['maps'][role] = {'path': converted['outputs'][role], 'source_key': key, 'srgb': role in ('BaseColor', 'SpecularColor', 'Emissive'), 'uv_scale': uv}
                if tex.get('resolution') == 'cinematic_branch_same_dorado_relative_path':
                    row['notes'].append(role+': original path unavailable; recovered exact Dorado-relative path from cinematic environment branch. Visual equivalence requires comparison.')
                if role == 'Normal' and 'Roughness' in converted['outputs']:
                    row['maps']['Roughness'] = {'path': converted['outputs']['Roughness'], 'source_key': key, 'srgb': False, 'uv_scale': uv}
                if role == 'BaseColor' and 'Opacity' in converted['outputs']:
                    row['maps']['Opacity'] = {'path': converted['outputs']['Opacity'], 'source_key': key, 'srgb': False, 'uv_scale': uv}
            elif tex['original'] != 'nearest_cubemap':
                row['notes'].append('Unresolved '+tex['slot']+': '+tex['original'])
        if row['metallic']:
            row['notes'].append('Metallic scalar inferred from original surface type/name; mixed surfaces may need a metal mask.')
        if 'SpecularColor' in row['maps']:
            row['notes'].append('Original colored specular retained; specular-gloss to metallic-roughness is an integration approximation.')
        if 'BlendMask' in row['maps']:
            row['notes'].append('CryEngine Opacity texture is a material layer blend mask; do not assume it means transparency.')
        if props.get('StringGenMask'):
            row['source_shader_flags'] = props['StringGenMask']
        if 'Opacity' in row['maps']:
            row['opacity_semantics'] = 'Recovered diffuse alpha; may be tint or blend mask. Enable transparency only when source shader/geometry requires it.'
        update_material_metadata(row, props)
        convert_specular_workflow(row, props, output)
        materials.append(row)
    manifest = {'version': 1, 'conversion': {'pillow': Image.__version__, 'max_dimension': args.max_dimension, 'normal_convention': 'DirectX X+ Y- Z+; reconstructed BC5 Z and normalized vectors; no green flip', 'roughness': '1 - recovered gloss, linear', 'source_read_only': True, 'references': ['https://www.cryengine.com/docs/static/engines/cryengine-3/categories/1114113/pages/1310833', 'https://www.cryengine.com/docs/static/engines/cryengine-3/categories/1114113/pages/1310821', 'https://www.cryengine.com/docs/static/engines/cryengine-3/categories/1114113/pages/21267786', 'https://raw.githubusercontent.com/KhronosGroup/glTF/main/extensions/2.0/Archived/KHR_materials_pbrSpecularGlossiness/examples/convert-between-workflows/js/three.pbrUtilities.js']}, 'materials': materials, 'textures': textures, 'errors': errors, 'summary': {'materials': len(materials), 'textures_prepared': len(textures), 'textures_failed': len(errors), 'png_count': len(list((output/'Prepared').glob('*.png')))+len(list((output/'PreparedPBR').glob('*.png'))), 'materials_with_pbr_conversion': sum('Metallic' in m['maps'] for m in materials)}}
    (output/'texture_manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps(manifest['summary']))
    if errors:
        print(json.dumps(errors[:10]))


if __name__ == '__main__':
    main()
