"""Check recovered PNG integrity, normal vectors, and gloss/roughness agreement."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest', default='SourceArt/Dorado/Textures/texture_manifest.json')
    parser.add_argument('--output', default='Saved/Logs/dorado-reconstruction-20261030/texture-validation.json')
    args = parser.parse_args()
    report = json.loads(Path(args.manifest).read_text())
    failures = []
    referenced = {p['path'] for m in report['materials'] for p in m['maps'].values()}
    for path in sorted(referenced):
        try:
            with Image.open(path) as image:
                image.verify()
        except Exception as exc:
            failures.append({'path': path, 'error': str(exc)})
    normal_errors = []
    gloss_pairs = 0
    for texture in report['textures']:
        maps = texture['outputs']
        if 'Normal' in maps:
            normal = np.asarray(Image.open(maps['Normal']), dtype=np.float32)/127.5-1
            deviation = float(np.max(np.abs(np.linalg.norm(normal, axis=-1)-1)))
            normal_errors.append(deviation)
            if deviation > .014:
                failures.append({'path': maps['Normal'], 'normal_length_error': deviation})
        if 'Roughness' in maps and 'Gloss' in maps:
            rough = np.asarray(Image.open(maps['Roughness']), dtype=np.int16)
            gloss = np.asarray(Image.open(maps['Gloss']), dtype=np.int16)
            if np.any(rough+gloss != 255):
                failures.append({'path': maps['Roughness'], 'error': 'roughness + gloss is not 255'})
            gloss_pairs += 1
    result = {'status': 'PASS' if not failures and not report['errors'] else 'FAIL', 'scope': 'Source texture recovery and encoding only; Unreal import, color-space readback and rendered tangent validation still required.', 'unique_referenced_pngs': len(referenced), 'normal_maps_checked': len(normal_errors), 'maximum_normal_length_error': max(normal_errors, default=0), 'gloss_roughness_pairs': gloss_pairs, 'failures': failures, 'lower_mip_recoveries': [t['source_key'] for t in report['textures'] if any('highest mip missing' in note for note in t['notes'])], 'unresolved_material_references': sum(len([n for n in m['notes'] if n.startswith('Unresolved')]) for m in report['materials'])}
    Path(args.output).write_text(json.dumps(result, indent=2))
    print(json.dumps(result))
    if result['status'] != 'PASS':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
