"""Audit staged Dorado regeneration inputs without opening Unreal or Blender."""
import datetime
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT.parents[1]


def main():
    required = [ROOT/p for p in [
        'Dorado_Recovered.blend', 'Dorado_RingEnergy_Reconstructed.blend',
        'dimensions.json', 'geometry_manifest.json', 'energy_manifest.json',
        'calibration_manifest.json', 'Config/geometry_stage.json',
        'Config/material_art_overrides.json', 'Config/backdrop_budget.json',
        'Config/inferred_glow_points.json', 'Scripts/import_dorado.py',
        'Scripts/dorado_dimensions.py', 'Scripts/configure_nanite_backdrop.py',
        'Scripts/prepare_geometry.py', 'Scripts/prepare_textures.py',
        'Scripts/build_ring_energy.py', 'Scripts/validate_dorado.py',
        'Scripts/validate_traversal.py', 'Textures/texture_manifest.json',
    ]]
    required.extend(ROOT/'Textures/Defaults'/(name+'.png') for name in
                    ['BaseColor', 'Normal', 'Roughness', 'SpecularColor', 'Metallic', 'Opacity', 'Emissive'])
    # CGF conversion consumes the sanitized MTL beside the owned CGF. The
    # separately preserved original MTL supplies inspectable source provenance.
    stage = json.loads((ROOT/'Config/geometry_stage.json').read_text())
    mtl_checks = {}
    for asset in stage['assets']:
        if not asset.get('source_mtl'):
            continue
        original = ROOT/'Sources/MTL'/Path(asset['source_mtl']).name
        converter = Path(asset['owned_cgf']).parent/Path(asset['source_mtl']).name
        required.extend([original, converter])
        key = str(converter)
        if key not in mtl_checks:
            mtl_checks[key] = {'original_mtl': str(original), 'converter_mtl': key,
                               'original_sha256': hashlib.sha256(original.read_bytes()).hexdigest() if original.is_file() else None,
                               'converter_sha256': hashlib.sha256(converter.read_bytes()).hexdigest() if converter.is_file() else None,
                               'asset_ids': []}
        mtl_checks[key]['asset_ids'].append(asset['id'])
    fbx_checks = []
    for filename in ['geometry_manifest.json', 'energy_manifest.json', 'calibration_manifest.json']:
        data = json.loads((ROOT/filename).read_text(encoding='utf-8-sig'))
        for asset in data['assets']:
            for key in ('fbx', 'dae', 'owned_cgf'):
                if asset.get(key):
                    required.append(Path(asset[key]))
            if asset.get('fbx'):
                path = Path(asset['fbx'])
                actual = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
                expected = asset.get('fbx_sha256')
                fbx_checks.append({'asset_id': asset['id'], 'path': str(path),
                                   'actual_sha256': actual, 'recorded_sha256': expected,
                                   'matches_recorded_hash': actual == expected if expected else None})
    textures = json.loads((ROOT/'Textures/texture_manifest.json').read_text())
    texture_paths = {Path(entry['path']) for material in textures['materials']
                     for entry in material.get('maps', {}).values() if entry.get('path')}
    required.extend(texture_paths)
    overrides = json.loads((ROOT/'Config/material_art_overrides.json').read_text())
    override_textures = []
    for override in overrides.get('overrides', {}).values():
        override_textures.extend(Path(path) for path in override.get('textures', {}).values())
    required.extend(override_textures)
    unique = sorted(set(required), key=str)
    missing = [str(path) for path in unique if not path.is_file()]
    outside = [str(path) for path in unique if not path.is_relative_to(ROOT)]
    native = PROJECT/'Plugins/GameFeatures/BreakawayCore/Content/Environments/Dorado'
    output = {'generated_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'scope': 'Read-only staged input existence and recorded FBX hash audit. No editor, regeneration execution, shader, runtime, or relocation acceptance.',
              'required_files_checked': len(unique), 'prepared_texture_paths': len(texture_paths),
              'art_override_texture_paths': len(override_textures), 'missing_files': missing,
              'required_paths_outside_source_package': outside,
              'staged_mtl_pairs': len(mtl_checks), 'staged_mtl_checks': list(mtl_checks.values()),
              'fbx_hash_checks': fbx_checks, 'fbx_paths_fingerprinted': len(fbx_checks),
              'fbx_recorded_hashes_checked': sum(bool(x['recorded_sha256']) for x in fbx_checks),
              'fbx_without_recorded_hash': sum(not x['recorded_sha256'] for x in fbx_checks),
              'fbx_hash_mismatch_count': sum(x['matches_recorded_hash'] is False for x in fbx_checks),
              'native_uasset_files_on_disk': len(list(native.rglob('*.uasset'))),
              'native_umap_files_on_disk': len(list(native.rglob('*.umap'))),
              'notes': ['Source/archive provenance paths are intentionally not treated as runtime or staged-rebuild dependencies.',
                        'Manifests contain workstation absolute paths. Relocating SourceArt requires rebasing those paths.',
                        'A fresh native rebuild generates material-import.json and geometry-import.json before assembly.',
                        'Original archive re-extraction requires external F:/ inputs and is separate from staged FBX/native regeneration.']}
    output['status'] = 'PASS_STAGED_FILE_AUDIT' if not missing and not outside and not output['fbx_hash_mismatch_count'] else 'REVIEW_REQUIRED'
    target = PROJECT/'Saved/Logs/dorado-reconstruction-20261030/source-package-path-audit.json'
    target.write_text(json.dumps(output, indent=2)+'\n')
    print(json.dumps({k:v for k,v in output.items() if k not in ('fbx_hash_checks', 'staged_mtl_checks', 'notes')}))


if __name__ == '__main__':
    main()
