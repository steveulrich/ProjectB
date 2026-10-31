"""Summarize completed Unreal CSV captures without importing Unreal.

UE appends new stats during a capture and repeats the full header at the end.
This parser requires the completion footer and uses that final header, retaining
missing samples explicitly instead of shifting columns or treating them as zero.
"""

import argparse
import csv
import datetime
import hashlib
import json
import math
from pathlib import Path
import statistics

PROJECT = Path(__file__).resolve().parents[3]
TIMINGS = ('FrameTime', 'GameThreadTime', 'RenderThreadTime', 'RHIThreadTime',
           'GPUTime', 'GameThreadTime_CriticalPath', 'RenderThreadTime_CriticalPath', 'MaxFrameTime')
COUNTERS = ('GPUMem/LocalBudgetMB', 'GPUMem/LocalUsedMB', 'GPUMem/SystemBudgetMB',
            'GPUMem/SystemUsedMB', 'TransientMemoryUsedMB', 'TransientMemoryAliasedMB',
            'ShadowCacheUsageMB', 'LightCount/UpdatedShadowMaps', 'GPUSceneInstanceCount',
            'DrawCall/ShadowDepths', 'DrawCall/Basepass', 'DrawCall/Translucency',
            'RHI/DrawCalls', 'DynamicResolutionVSMNanite', 'DynamicNaniteScalingShadow',
            'DynamicTranslucencyResolution')
META_KEYS = ('platform', 'config', 'engineversion', 'os', 'cpu', 'gpu', 'gpudriver',
             'deviceprofile', 'systemresolution.resx', 'systemresolution.resy', 'rhiname',
             'rhifeaturelevel', 'shaderplatform', 'raytracing', 'streamingpoolsizemb',
             'targetframerate', 'vsyncenabled', 'starttimestamp', 'endtimestamp', 'captureduration')


def percentile(values, fraction):
    """Linear interpolation between sorted samples, the R-7 convention."""
    ordered = sorted(values)
    position = (len(ordered)-1)*fraction
    low = math.floor(position)
    high = math.ceil(position)
    return ordered[low]+(ordered[high]-ordered[low])*(position-low)


def summarize(values, frame_count):
    if not values:
        return {'samples': 0, 'missing_samples': frame_count}
    return {'samples': len(values), 'missing_samples': frame_count-len(values),
            'zero_samples': sum(v == 0 for v in values),
            'mean': round(statistics.fmean(values), 6),
            'p50': round(percentile(values, .50), 6),
            'p95': round(percentile(values, .95), 6),
            'p99': round(percentile(values, .99), 6),
            'min': round(min(values), 6), 'max': round(max(values), 6)}


def analyze(path):
    payload = path.read_bytes()
    rows = list(csv.reader(payload.decode('utf-8-sig').splitlines()))
    if not rows or not rows[-1] or rows[-1][0] != '[HasHeaderRowAtEnd]':
        raise ValueError('Capture has no completion footer: '+str(path))
    metadata = {rows[-1][i].strip('[]'): rows[-1][i+1] for i in range(0, len(rows[-1])-1, 2)}
    headers = [row for row in rows if row and row[0] == 'EVENTS']
    if not headers or metadata.get('HasHeaderRowAtEnd') != '1':
        raise ValueError('Unsupported Unreal CSV header convention: '+str(path))
    header = headers[-1]
    if any(old != header[:len(old)] for old in headers):
        raise ValueError('CSV stat order changed during capture: '+str(path))
    if len(set(header)) != len(header):
        raise ValueError('Duplicate CSV stat names: '+str(path))
    frames = [row for row in rows if row and row[0] != 'EVENTS' and not row[0].startswith('[')]
    if any(len(row) > len(header) for row in frames):
        raise ValueError('Data row is longer than the final header.')
    columns = {}
    for index, name in enumerate(header):
        if name == 'EVENTS':
            continue
        values = []
        for row in frames:
            if index >= len(row) or row[index] == '':
                continue
            number = float(row[index])
            if not math.isfinite(number):
                raise ValueError('Nonfinite stat: '+name)
            values.append(number)
        columns[name] = values
    extra = [name for name in header if name.lower().startswith(('vsm/', 'gpu/', 'lumen/'))]
    selected = list(dict.fromkeys(TIMINGS+COUNTERS+tuple(extra)))
    stats = {name: summarize(columns[name], len(frames)) for name in selected if name in columns}
    frame_times = columns.get('FrameTime', [])
    frame_index = header.index('FrameTime')
    worst = sorted(((float(row[frame_index]), index) for index, row in enumerate(frames)), reverse=True)[:5]
    return {'source': str(path.resolve()), 'sha256': hashlib.sha256(payload).hexdigest(),
            'completed': True, 'frames': len(frames), 'final_stat_columns': len(header)-1,
            'metadata': {key: metadata.get(key) for key in META_KEYS},
            'timing_unit': 'milliseconds', 'counter_units': 'As named; MB labels retained without reinterpretation.',
            'stats': stats, 'missing_requested_columns': [name for name in TIMINGS+COUNTERS if name not in columns],
            'explicit_vsm_counter_columns': [name for name in extra if name.lower().startswith('vsm/')],
            'lumen_counter_columns': [name for name in extra if name.lower().startswith('lumen/')],
            'gpu_pass_timing_columns': [name for name in extra if name.lower().startswith('gpu/')],
            'frame_threshold_counts': {str(limit): sum(value > limit for value in frame_times) for limit in (16.67, 33.33, 50.0)},
            'slowest_frames': [{'frame_index_zero_based': index, 'frame_ms': value} for value, index in worst]}


def comparison(before, after):
    fields = ('platform', 'config', 'engineversion', 'cpu', 'gpu', 'gpudriver',
              'deviceprofile', 'systemresolution.resx', 'systemresolution.resy',
              'rhiname', 'rhifeaturelevel', 'shaderplatform', 'raytracing', 'vsyncenabled')
    differences = {key: [before['metadata'].get(key), after['metadata'].get(key)]
                   for key in fields if before['metadata'].get(key) != after['metadata'].get(key)}
    deltas = {}
    for name in before['stats'].keys() & after['stats'].keys():
        a, b = before['stats'][name], after['stats'][name]
        if not a['samples'] or not b['samples']:
            continue
        deltas[name] = {stat: {'absolute': round(b[stat]-a[stat], 6),
                              'percent': round((b[stat]/a[stat]-1)*100, 3) if a[stat] else None}
                        for stat in ('mean', 'p50', 'p95', 'p99', 'max')}
    return {'before': Path(before['source']).name, 'after': Path(after['source']).name,
            'same_frame_count': before['frames'] == after['frames'],
            'capture_metadata_differences': differences, 'delta_after_minus_before': deltas}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', nargs='*', type=Path)
    parser.add_argument('--output', type=Path,
                        default=PROJECT/'Saved/Logs/dorado-reconstruction-20261030/performance-csv-comparison.json')
    args = parser.parse_args()
    paths = args.captures or [PROJECT/'Saved/Profiling/CSV'/name for name in
        ('dorado-baseline-fixed.csv', 'dorado-backdrop-gi.csv', 'dorado-backdrop-shadow.csv')]
    captures, pending = [], []
    for path in paths:
        if not path.exists():
            pending.append({'source': str(path), 'reason': 'not present'})
            continue
        try:
            captures.append(analyze(path))
        except ValueError as exc:
            pending.append({'source': str(path), 'reason': str(exc)})
    report = {'generated_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'scope': 'Single fixed-camera PIE Development captures in WindowsEditor; not packaged or shipping performance.',
        'declared_by_root_not_embedded_in_csv': {'map': '/BreakawayCore/Environments/Dorado/Maps/L_Dorado',
            'character_location_cm': [450, 1000, 98], 'camera_yaw_degrees': 90,
            'screen_percentage': 100, 'dynamic_resolution_operation_mode': 0, 'scalability': 'Epic, all groups3',
            'expected_frames_per_capture': 600},
        'percentile_method': 'R-7 linear interpolation; all valid recorded frames retained; no outlier trimming.',
        'captures': captures, 'pending_or_rejected': pending,
        'comparisons': [comparison(captures[i-1], captures[i]) for i in range(1, len(captures))],
        'limitations': ['One capture per condition; order, warm-up and background-editor noise can affect differences.',
            'Frame and thread timing do not alone identify the critical-path cause; GPU timing may contain synchronization.',
            'Primary screen percentage, dynamic-resolution mode and camera are operator declarations, not CSV-verified fields.',
            'Absence of explicit VSM/Lumen counters cannot prove warning removal; use the corresponding fresh UI evidence.',
            'DynamicResolutionVSMNanite is a shadow/Nanite scaling counter, not verification of the primary view dynamic-resolution mode.']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    summary = {'scope': report['scope'], 'detailed_report': str(args.output),
               'percentile_method': report['percentile_method'], 'captures': [],
               'limitations': report['limitations']}
    for capture in captures:
        summary['captures'].append({
            'file': Path(capture['source']).name,
            'frames': capture['frames'],
            'timings_ms': {name: {stat: capture['stats'][name][stat] for stat in ('mean', 'p50', 'p95', 'p99')}
                           for name in ('FrameTime', 'GameThreadTime', 'GPUTime') if name in capture['stats']},
            'counter_medians': {name: capture['stats'][name].get('p50') for name in
                                ('GPUMem/LocalUsedMB', 'GPUMem/LocalBudgetMB', 'RHI/DrawCalls', 'DrawCall/ShadowDepths')
                                if name in capture['stats']},
            'frames_over_threshold_ms': capture['frame_threshold_counts'],
            'vsm_counter_columns': capture['explicit_vsm_counter_columns'],
            'lumen_counter_columns': capture['lumen_counter_columns'],
            'gpu_pass_timing_columns': capture['gpu_pass_timing_columns'],
        })
    summary_path = args.output.with_name(args.output.stem+'-summary.json')
    summary_path.write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
    for capture in captures:
        print(json.dumps({'capture': Path(capture['source']).name, 'frames': capture['frames'],
            'FrameTime': capture['stats'].get('FrameTime'), 'GameThreadTime': capture['stats'].get('GameThreadTime'),
            'GPUTime': capture['stats'].get('GPUTime'), 'VRAM_MB': capture['stats'].get('GPUMem/LocalUsedMB')}))
    print(json.dumps({'report': str(args.output), 'summary': str(summary_path), 'pending': pending}))


if __name__ == '__main__':
    main()
