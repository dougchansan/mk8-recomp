"""Summarize bounded log timings; GPU timestamps are asynchronous work, not display scanout."""
import argparse
import json
import re
import statistics
from pathlib import Path

PATTERNS = {
    'presentation': (r'native delivery over 60: wall ([\d.]+) ms/frame, caller CPU ([\d.]+), peak ([\d.]+), over20ms (\d+).*?, min ([\d.]+) ms',
                     ('interval_ms', 'caller_cpu_ms', 'worst_interval_ms', 'over20ms_count', 'best_interval_ms')),
    'render_thread': (r'nvn/vk: per frame over 60: [\d.]+ fps, [\d.]+ submits, replay ([\d.]+) ms, submit \(encode\) ([\d.]+) ms, fence wait ([\d.]+) ms, other ([\d.]+) ms, present ([\d.]+) ms, upload ([\d.]+) MB',
                      ('replay_ms', 'encode_ms', 'fence_wait_ms', 'other_ms', 'present_ms', 'upload_MB')),
    'gpu': (r'gpu busy: ([\d.]+) ms/frame \([\d.]+ parts/frame\), span ([\d.]+) ms/frame, (\d+) not ready;',
            ('execution_ms', 'slot_span_ms', 'not_ready_count')),
    'gpu_classes': (r'gpu busy classes: replay ([\d.]+) ms/frame \([\d.]+ parts/frame\), present ([\d.]+) ms/frame',
                    ('replay_ms', 'present_ms')),
}


def summarize(text, bounded=False):
    result = {}
    for name, (pattern, fields) in PATTERNS.items():
        rows = [tuple(map(float, m.groups())) for m in re.finditer(pattern, text)]
        if bounded:
            rows = rows[1:]  # independently discard each capture-adjacent bucket
        metrics = {}
        for i, field in enumerate(fields):
            values = [r[i] for r in rows]
            if values:
                metrics[field] = dict(mean=statistics.mean(values), min=min(values), max=max(values))
                if field.endswith('_count'):
                    metrics[field]['total'] = sum(values)
        result[name] = dict(windows=len(rows), metrics=metrics)
    gpu = result['gpu']
    result['gpu_status'] = ('missing' if not gpu['windows'] else
                            'incomplete' if gpu['metrics']['not_ready_count']['total'] else 'available')
    return result



FRAME = re.compile(r'measure frame: present (\d+) submit-through (\d+) start (\d+) end (\d+) cpu (\d+) tid (-?\d+)')
GPU_PART = re.compile(r'measure gpu: submit (\d+) present (\d+) slot (\d+) part (\d+) ready (\d+) duration (\d+)')
EVENT = re.compile(r'measure event: ([\w-]+) id (\d+) submit (\d+) present (\d+) epoch (\d+) start (\d+) end (\d+) value (\d+) tid (-?\d+)')
RENDER = re.compile(r'measure render: submit (\d+) replay (\d+) encode (\d+) fence (\d+) upload (\d+)')


def distribution(values):
    """Nearest-rank percentiles over individual observations, never bucket means."""
    import math
    values = sorted(values)
    if not values:
        return None
    return dict(count=len(values), mean=statistics.mean(values), min=values[0], max=values[-1],
                **{f'p{p}': values[max(0, math.ceil(p / 100 * len(values)) - 1)] for p in (50, 95, 99)})


def detailed(text, context=None):
    # Full context may include delayed GPU retirements beyond the racing capture.
    context = text if context is None else context
    frames = [tuple(map(int, m.groups())) for m in FRAME.finditer(text)]
    intervals = [(r[3] - r[2]) / 1e6 for r in frames if r[3] >= r[2]]
    gpu, present_gpu, unavailable = {}, {}, 0
    invalid_submits, invalid_presents = set(), set()
    for m in GPU_PART.finditer(context):
        submit, present, slot, part, ready, ns = map(int, m.groups())
        if not ready:
            unavailable += 1
            (invalid_submits if submit else invalid_presents).add(submit if submit else present)
            continue
        dest, key = (gpu, submit) if submit else (present_gpu, present)
        dest[key] = dest.get(key, 0) + ns
    render = {int(m[1]): tuple(map(int, m.groups()[1:])) for m in RENDER.finditer(context)}
    events = [(m[1], *map(int, m.groups()[1:])) for m in EVENT.finditer(context)]
    triggers, assets = {}, {}
    asset_paths = {}
    for path in re.findall(r'fs: OpenFile\((.*?)(?:, \d+)?\) ', context):
        h = 14695981039346656037
        for byte in path.encode('utf-8'):
            h = ((h ^ byte) * 1099511628211) & ((1 << 64) - 1)
        asset_paths[h] = path
    # Host-time intersection shows coincidence, not proof of a cause.
    hitches = []
    for row in sorted(frames, key=lambda f: f[3]-f[2], reverse=True)[:50]:
        present, through, start, end, cpu, tid = row
        if end - start <= 33_333_333:
            continue
        overlap = {}
        for kind, asset, submit, job_present, epoch, a, b, value, event_tid in events:
            ns = max(0, min(end, b) - max(start, a))
            if ns:
                overlap[kind] = overlap.get(kind, 0) + ns / 1e6
        hitches.append(dict(present=present, interval_ms=(end-start)/1e6,
                            caller_cpu_ms=cpu/1e6, overlapping_events_ms=overlap))
    import bisect
    ranges = sorted((f[2], f[3]) for f in frames)
    ends = [b for a,b in ranges]
    for kind, asset, submit, present, epoch, start, end, value, tid in events:
        i = bisect.bisect_right(ends, start)
        if not ranges or i == len(ranges) or ranges[i][0] >= end:
            continue
        item = triggers.setdefault(kind, dict(count=0, duration_ms=0, value_total=0))
        item['count'] += 1
        item['duration_ms'] += max(0, end-start)/1e6
        item['value_total'] += value
        if kind == 'asset-read':
            a = assets.setdefault(asset, dict(asset_id=asset, path=asset_paths.get(asset),
                                             reads=0, requested_bytes=0, wall_ms=0))
            a['reads'] += 1
            a['requested_bytes'] += value
            a['wall_ms'] += max(0,end-start)/1e6
    # Only complete submit ranges bounded by two observed presentation watermarks.
    correlated = []
    for previous, current in zip(frames, frames[1:]):
        a, b = previous[1], current[1]
        if b < a or b-a > 10000:
            continue
        ids = list(range(a+1, b+1))
        correlated.append(dict(present=current[0], submit_ids=ids,
            gpu_ms=sum(gpu[i] for i in ids)/1e6 if ids and all(i in gpu and i not in invalid_submits for i in ids) else None,
            replay_ms=sum(render[i][0] for i in ids)/1e6 if ids and all(i in render for i in ids) else None,
            present_gpu_ms=present_gpu.get(current[0], None)/1e6 if current[0] in present_gpu and current[0] not in invalid_presents else None))
    return dict(individual_intervals_ms=distribution(intervals),
                above_ms={str(t): sum(v > t for v in intervals) for t in (16.666667, 33.333333, 100)},
                caller_cpu_ms=distribution([r[4]/1e6 for r in frames]),
                missing_present_ids=sum(max(0,b[0]-a[0]-1) for a,b in zip(frames,frames[1:])),
                truncated='measure: truncated' in context, unavailable_gpu_parts=unavailable,
                gpu_correlated_presentations=correlated, events=triggers,
                asset_reads=sorted(assets.values(), key=lambda a:a['wall_ms'], reverse=True)[:50],
                largest_hitches=sorted(hitches,key=lambda h:h['interval_ms'],reverse=True)[:50],
                correlation_limit='Submit watermark groups are CPU call order, not proof of simulation-frame ownership; GPU durations are not host timestamps.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('case', type=Path)
    parser.add_argument('--events', nargs=2, type=int, metavar=('FIRST', 'LAST'),
                        help='One-based captures independently verified as racing on the same course')
    args = parser.parse_args()
    if args.events and (args.events[0] < 1 or args.events[1] <= args.events[0]):
        parser.error('Events must be increasing positive one-based indices')
    raw = (args.case / 'native.log').read_bytes()
    context = raw.decode('utf-8', errors='replace')
    if args.events:
        events = json.loads((args.case / 'result.json').read_text())['events']
        a, b = (events[n - 1]['native_log_bytes'] for n in args.events)
        if not 0 < a < b <= len(raw):
            raise ValueError('Invalid capture byte boundaries')
        raw = raw[a:b]
    report = summarize(raw.decode('utf-8', errors='replace'), bool(args.events))
    report['individual'] = detailed(raw.decode('utf-8', errors='replace'), context)
    config = args.case / 'run-config.json'
    report['run_config'] = json.loads(config.read_text(encoding='utf-8')) if config.exists() else None
    report['correctness'] = dict(fatal_lines=[l for l in context.splitlines() if l.startswith('FATAL')],
                                shader_miss_lines=[l for l in context.splitlines() if l.startswith('nvn: shader ') and ': no ' in l],
                                simulation_speed='Requires reviewed engine-class and lap progression captures; not inferred from FPS')
    closed_path = args.case / 'result.json'
    if closed_path.exists():
        closed = json.loads(closed_path.read_text())
        report['correctness']['closed_run'] = {k:v for k,v in closed.items() if k != 'events'}
        report['correctness']['capture_events'] = len(closed.get('events', []))
        report['resource_samples'] = [{k:e[k] for k in ('t','resources','capture_dimensions') if k in e}
                                      for e in closed.get('events', []) if e.get('resources')]
    report['adapter_and_surface'] = [l for l in context.splitlines()
        if 'using GPU ' in l or 'swapchain' in l or 'surface:' in l][:30]
    report['scope'] = 'HUD-verified racing captures required' if args.events else 'whole log including menus/loading'
    report['limits'] = ('Aggregate sections use independent 60-present buckets; individual section uses recorded frame intervals. '
                        'GPU execution sums timestamped command parts, including stalls within them; slot span includes gaps. '
                        'Retirement can lag presentation boundaries. Timings overlap; do not add them. '
                        'No display scanout measurement. Compare plain/GPU runs to assess observer overhead.')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
