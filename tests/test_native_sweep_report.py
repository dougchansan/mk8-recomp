import unittest
from scripts.native_sweep_report import parse


class ReportTests(unittest.TestCase):
    def test_cap_and_course_boundary_do_not_become_zero_or_double_counts(self):
        result = parse([
            'OpenFile(content:/Course/First/course_model.szs)',
            'slow frame: present 1 interval 300.00 ms: cpu 15.62, worker-wait 0.00, kernel 200.00, other 84.38 | worker: pipelines 2 (4.00 ms), upload 10.00 MB',
            'slow frames over 600: 100 over 20.0 ms; their totals: cpu 10.0, kernel 200.0, other 50.0 ms',
            'slow frames over 600: woken by: g50 0 200.0 ms',
            'OpenFile(content://Course/Second/course_model.szs)',
            'slow frames over 600: 200 over 20.0 ms; their totals: cpu 20.0, kernel 400.0, other 50.0 ms',
            'slow frames over 600: 300 over 20.0 ms; their totals: cpu 30.0, kernel 600.0, other 50.0 ms',
        ])
        first, second = result['sections']
        self.assertEqual(first['contained_summary_late_frames'], 100)
        self.assertEqual(second['contained_summary_late_frames'], 300)
        self.assertEqual(second['detailed_late_frames'], 0)
        self.assertIsNone(second['worst_observed_ms'])
        self.assertEqual(second['boundary_summary_indices'], [1])
        self.assertEqual(result['summary_buckets'][1]['section_indices'], [0, 1])
        self.assertIn('g50', result['summary_buckets'][0]['woken_by'])
        self.assertEqual(first['worst_observed_ms'], 300)
        worker = first['largest_observed_intervals'][0]['worker_observations']
        self.assertEqual(worker['pipelines'], 2)
        self.assertEqual(worker['pipelines_ms'], 4)
        self.assertEqual(worker['upload'], 10)

    def test_empty_and_repeated_load_and_negative_other(self):
        self.assertEqual(parse([])['sections'], [])
        result = parse(['OpenFile(content:/Course/A/course_model.szs)'] * 2 + [
            'nvn/vk: per frame over 60: 60.5 fps',
            'slow frame: present 9 interval 20.10 ms: cpu 31.25, other -11.15 | worker: upload 1.00 MB',
            'FATAL example'])
        self.assertEqual(len(result['sections']), 1)
        section = result['sections'][0]
        self.assertEqual(section['median_fps'], 60.5)
        self.assertEqual(section['largest_observed_intervals'][0]['presenter']['other'], -11.15)
        self.assertEqual(section['fatal'], ['FATAL example'])


if __name__ == '__main__':
    unittest.main()
