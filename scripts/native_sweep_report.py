"""Report CLOSED sweeps without mistaking the detailed-log cap for zero hitches.

Usage: python scripts/native_sweep_report.py <closed-drive-dir>
600-present summaries can straddle course loads: keep those buckets separate.
Worker counters overlap presenter time; they are observations, not causal costs.
"""
import argparse
import json
import re
import statistics
from pathlib import Path

COURSE = re.compile(r'OpenFile\(content:/+Course/(\w+)/course_model\.szs\)')
FPS = re.compile(r'^nvn/vk: per frame over 60: ([\d.]+) fps')
DETAIL = re.compile(r'^slow frame: present (\d+) interval ([\d.]+) ms: (.*)')
SUMMARY = re.compile(r'^slow frames over 600: (\d+) over ([\d.]+) ms; their totals: (.*)')
FIELD = re.compile(r'([a-z][a-z-]*) (-?[\d.]+)')
TIMED_WORK = re.compile(r'(pipelines|astc) \d+ \(([\d.]+) ms\)')
DELIVERY = re.compile(r'^native delivery over 60: wall ([\d.]+) ms/frame, caller CPU [\d.]+, peak ([\d.]+), over20ms (\d+)(.*)')
MINIMUM = re.compile(r', min ([\d.]+) ms\s*$')


def parse(lines):
    sections, buckets, delivery = [], [], []
    current = None
    pending_courses = []
    pending_delivery_courses = []
    for number, line in enumerate(lines, 1):
        course = COURSE.search(line)
        name = course[1] if course else ('boot/menus' if current is None else current['course'])
        if current is None or name != current['course']:
            current = dict(course=name, start_line=number, fps=[], details=[], fatal=[], shader_misses=[])
            sections.append(current)
            pending_courses.append(len(sections) - 1)
            pending_delivery_courses.append(len(sections) - 1)
        if match := FPS.match(line):
            current['fps'].append(float(match[1]))
        elif match := DETAIL.match(line):
            presenter, _, worker = match[3].partition(' | worker: ')
            observations = dict((k, float(v)) for k, v in FIELD.findall(worker))
            observations.update((k + '_ms', float(v)) for k, v in TIMED_WORK.findall(worker))
            current['details'].append(dict(present=int(match[1]), interval_ms=float(match[2]),
                presenter=dict((k, float(v)) for k, v in FIELD.findall(presenter)),
                worker_observations=observations))
        elif match := SUMMARY.match(line):
            buckets.append(dict(section_indices=list(pending_courses), late_frames=int(match[1]),
                threshold_ms=float(match[2]), presenter_totals_ms=dict((k, float(v)) for k, v in FIELD.findall(match[3]))))
            pending_courses = [len(sections) - 1]
        elif match := DELIVERY.match(line):
            minimum = MINIMUM.search(match[4])
            delivery.append(dict(section_indices=list(pending_delivery_courses),
                mean_ms=float(match[1]), maximum_ms=float(match[2]), late_frames=int(match[3]),
                minimum_ms=float(minimum[1]) if minimum else None))
            pending_delivery_courses = [len(sections) - 1]
        elif line.startswith('slow frames over 600: woken by:') and buckets:
            buckets[-1]['woken_by'] = line.partition('woken by:')[2].strip()
        elif line.startswith('FATAL'):
            current['fatal'].append(line.strip())
        elif line.startswith('nvn: shader ') and ': no ' in line:
            current['shader_misses'].append(line.strip())
    for index, section in enumerate(sections):
        fps = section.pop('fps')
        section['fps_windows'] = len(fps)
        section['median_fps'] = statistics.median(fps) if fps else None
        section['min_fps'] = min(fps) if fps else None
        section['detailed_late_frames'] = len(section['details'])
        section['worst_observed_ms'] = max((d['interval_ms'] for d in section['details']), default=None)
        section['contained_summary_late_frames'] = sum(b['late_frames'] for b in buckets if b['section_indices'] == [index])
        section['boundary_summary_indices'] = [i for i, b in enumerate(buckets) if index in b['section_indices'] and len(b['section_indices']) > 1]
        contained = [b for b in delivery if b['section_indices'] == [index]]
        section['delivery_windows'] = len(contained)
        section['minimum_present_ms'] = min((b['minimum_ms'] for b in contained if b['minimum_ms'] is not None), default=None)
        section['maximum_present_ms'] = max((b['maximum_ms'] for b in contained), default=None)
        section['boundary_delivery_indices'] = [i for i, b in enumerate(delivery) if index in b['section_indices'] and len(b['section_indices']) > 1]
        section['largest_observed_intervals'] = sorted(section.pop('details'), key=lambda d: d['interval_ms'], reverse=True)[:10]
    return dict(sections=sections, summary_buckets=buckets, delivery_buckets=delivery, limitations=[
        'Detailed interval logs have a process-wide cap; absent details do not mean zero late frames.',
        'Summary counts and detailed counts overlap: never add them.',
        'Boundary buckets include multiple course/menu sections and cannot be assigned exactly.',
        'The final incomplete 600-present bucket may have no summary.',
        'Course-load sections include loading, racing and subsequent menus; FPS is diagnostic coverage.',
        'Worker observations overlap presenter time; uploads report bytes, not duration.',
        'Thread CPU ticks are coarse; CPU plus other does not distinguish execution from scheduling.',
        'Presentation minima require R284 delivery logs; legacy logs leave them unavailable. Boundary buckets are excluded.'
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('case', type=Path)
    args = parser.parse_args()
    if not (args.case / 'result.json').is_file():
        parser.error('Sweep is not closed (result.json absent); leave the running game undisturbed.')
    with (args.case / 'native.log').open(encoding='utf-8', errors='replace') as stream:
        result = parse(stream)
    result['case'] = str(args.case.resolve())
    target = args.case / 'attribution-report.json'
    target.write_text(json.dumps(result, indent=2), encoding='utf-8')
    for section in result['sections']:
        print(f"{section['course']:24} details={section['detailed_late_frames']:5} "
              f"contained-summary={section['contained_summary_late_frames']:6} "
              f"worst-observed-ms={section['worst_observed_ms']}")
        print(f"  contained delivery windows={section['delivery_windows']} min-ms={section['minimum_present_ms']} max-ms={section['maximum_present_ms']}")
    print(f'Limitations and boundary buckets: {target}')


if __name__ == '__main__':
    main()
