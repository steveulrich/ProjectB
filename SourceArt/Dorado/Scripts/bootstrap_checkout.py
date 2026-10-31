"""Seed only missing Dorado working reports from the committed starting snapshot.

This copies provenance/import registries, not validation results or PIE preferences.
It never opens Unreal, modifies native assets, or overwrites an existing report.
"""
import argparse
import hashlib
import json
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1]
ALLOWLIST = (
    'source-material-mapping.json', 'source-effects-scope.json',
    'map-baseline.json', 'inherited-frontend-builddata.json',
    'material-import.json', 'geometry-import.json',
    'scene-assembly.json', 'scene-assembly-energy.json',
    'geometry-shader-filter-changes.json', 'nanite-backdrop-configuration.json',
)


def bootstrap(project_root=None, dry_run=False):
    project = Path(project_root).resolve() if project_root else SOURCE.parents[1]
    if not (project / 'ProjectB.uproject').is_file():
        raise RuntimeError('Expected the intended ProjectB workspace.')
    provenance = SOURCE / 'Config/Provenance'
    snapshot = json.loads((provenance / 'snapshot.json').read_text(encoding='utf-8'))
    destination = project / 'Saved/Logs/dorado-reconstruction-20261030'
    results = []
    # Verify every committed input before copying any of them.
    payloads = {}
    for name in ALLOWLIST:
        payload = (provenance / name).read_bytes()
        if hashlib.sha256(payload).hexdigest() != snapshot['files'][name]['sha256']:
            raise RuntimeError('Provenance snapshot hash mismatch: ' + name)
        json.loads(payload.decode('utf-8-sig'))
        payloads[name] = payload
    for name, payload in payloads.items():
        target = destination / name
        if target.exists():
            status = 'preserved_existing'
        elif dry_run:
            status = 'would_copy_missing'
        else:
            destination.mkdir(parents=True, exist_ok=True)
            try:
                # Exclusive creation also preserves reports created concurrently.
                with target.open('xb') as stream:
                    stream.write(payload)
                status = 'copied_missing'
            except FileExistsError:
                status = 'preserved_existing'
        results.append({'file': name, 'status': status})
    return {'destination': str(destination), 'dry_run': dry_run,
            'scope': 'Known starting snapshot only; no new native validation.',
            'files': results}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-root', type=Path)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    print(json.dumps(bootstrap(args.project_root, args.dry_run), indent=2))
