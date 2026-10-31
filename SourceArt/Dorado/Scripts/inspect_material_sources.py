"""Read original Dorado XML materials and index source textures without modifying them."""
import argparse
import json
import os
import struct
import xml.etree.ElementTree as ET
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-root', default='F:/BreakawayConverted/Unpacked')
    parser.add_argument('--output', default='Saved/Logs/dorado-reconstruction-20261030/source-material-mapping.json')
    args = parser.parse_args()
    root = Path(args.source_root)
    texture_index = {}
    for part in sorted(root.glob('textures*')):
        for folder, _, files in os.walk(part):
            for name in files:
                if name.lower().endswith('.dds'):
                    path = Path(folder) / name
                    key = path.relative_to(part).as_posix().lower()
                    texture_index.setdefault(key, []).append(path)
    materials = []
    errors = []
    source_folders = [root/'objects-part0/objects/01_eldorado_a_temple', root/'gamedata/materials/01_eldorado_a_temple']
    for source_folder in source_folders:
        for path in sorted(source_folder.rglob('*.mtl')):
            try:
                tree = ET.parse(path)
                for material_index, element in enumerate(e for e in tree.iter('Material') if e.find('SubMaterials') is None):
                    if element.find('SubMaterials') is not None:
                        continue
                    row = {'file': str(path), 'submaterial_index': material_index, 'name': element.get('Name', path.stem), 'properties': dict(element.attrib), 'textures': [], 'publicParams': dict(element.find('PublicParams').attrib) if element.find('PublicParams') is not None else {}}
                    for tex in element.findall('Textures/Texture'):
                        original = tex.get('File', '')
                        key = str(Path(original.replace('\\', '/')).with_suffix('.dds')).replace('\\', '/').lower()
                        matches = texture_index.get(key, [])
                        resolution = 'exact_original_relative_path'
                        if not matches and key.startswith('objects/01_eldorado_a_temple/'):
                            alternate_key = key.replace('objects/01_eldorado_a_temple/', 'objects/cinematics/argus_cinematic/arg_eldo/environment_branch/01_eldorado_a_temple/', 1)
                            matches = texture_index.get(alternate_key, [])
                            if matches:
                                resolution = 'cinematic_branch_same_dorado_relative_path'
                        entry = {'slot': tex.get('Map'), 'original': original, 'resolution': resolution, 'texMod': dict(tex.find('TexMod').attrib) if tex.find('TexMod') is not None else {}, 'resolved': []}
                        for texture in matches:
                            header = texture.read_bytes()[:148]
                            info = {'path': str(texture), 'bytes': texture.stat().st_size, 'splitParts': [str(p) for p in sorted(texture.parent.glob(texture.name+'.*'))]}
                            if len(header) >= 128 and header[:4] == b'DDS ':
                                info.update(height=struct.unpack_from('<I', header, 12)[0], width=struct.unpack_from('<I', header, 16)[0], mipCount=struct.unpack_from('<I', header, 28)[0], fourCC=header[84:88].decode('ascii', errors='replace'))
                            entry['resolved'].append(info)
                        row['textures'].append(entry)
                    materials.append(row)
            except Exception as exc:
                errors.append({'path': str(path), 'error': str(exc)})
    placements = []
    for path in sorted((root/'levels/levels/01_eldorado_a_temple/layers').glob('*.lyr')):
        if not (path.stem.startswith('geo_') or path.stem in ('sys_spec_min_off', 'lights_high', 'lights_team_portal')):
            continue
        tree = ET.parse(path)
        objects = []
        for element in tree.iter('Object'):
            row = dict(element.attrib)
            if row.get('Prefab', '').lower().endswith('.cgf') or row.get('Type') in ('GeomEntity', 'Light'):
                row['sourceLayerFile'] = str(path)
                objects.append(row)
        placements.extend(objects)
    mapped = sum(bool(t['resolved']) for row in materials for t in row['textures'] if t['original'] != 'nearest_cubemap')
    unresolved = [{'material': row['name'], 'slot': t['slot'], 'original': t['original']} for row in materials for t in row['textures'] if not t['resolved'] and t['original'] != 'nearest_cubemap']
    report = {'sourceRoot': str(root), 'materials': materials, 'placements': placements, 'unresolved': unresolved, 'errors': errors, 'summary': {'materials': len(materials), 'mappedTextureReferences': mapped, 'unresolvedTextureReferences': len(unresolved), 'geometryPlacements': len(placements), 'indexedTextures': len(texture_index)}}
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report['summary']))


if __name__ == '__main__':
    main()
