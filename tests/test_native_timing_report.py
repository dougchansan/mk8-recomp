import importlib.util
import unittest
from pathlib import Path
spec = importlib.util.spec_from_file_location('timing', Path(__file__).parents[1] / 'scripts/native_timing_report.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class TimingReportTests(unittest.TestCase):
    def test_missing_gpu_is_not_zero(self):
        self.assertEqual(m.summarize('')['gpu_status'], 'missing')
        self.assertEqual(m.summarize('')['gpu']['metrics'], {})

    def test_boundary_and_unavailable_results(self):
        text = ('gpu busy: 99.00 ms/frame (2.0 parts/frame), span 99.00 ms/frame, 0 not ready;\n'
                'gpu busy: 4.90 ms/frame (2.0 parts/frame), span 6.00 ms/frame, 2 not ready;\n')
        r = m.summarize(text, True)
        self.assertEqual(r['gpu']['windows'], 1)
        self.assertEqual(r['gpu']['metrics']['execution_ms']['mean'], 4.9)
        self.assertEqual(r['gpu_status'], 'incomplete')

    def test_classes_are_separate_from_execution(self):
        text = ('gpu busy: 5.00 ms/frame (2.0 parts/frame), span 7.00 ms/frame, 0 not ready;\n'
                'gpu busy classes: replay 4.00 ms/frame (1.0 parts/frame), present 1.00 ms/frame (1.0 parts/frame)')
        r = m.summarize(text)
        self.assertEqual(r['gpu_status'], 'available')
        self.assertEqual(r['gpu_classes']['metrics']['replay_ms']['mean'], 4)
        self.assertEqual(r['gpu']['metrics']['execution_ms']['mean'], 5)


    def test_individual_percentiles_not_bucket_means(self):
        text = ''.join(f'measure frame: present {i+1} submit-through {i} start {i*1000000} end {i*1000000+n} cpu 10 tid 1\n'
                       for i,n in enumerate((10000000,20000000,40000000,120000000)))
        r = m.detailed(text)
        self.assertEqual(r['individual_intervals_ms']['p50'],20)
        self.assertEqual(r['individual_intervals_ms']['p95'],120)
        self.assertEqual(r['above_ms']['100'],1)

    def test_delayed_gpu_join_and_missing_data(self):
        frames = ('measure frame: present 2 submit-through 10 start 0 end 10000000 cpu 1 tid 1\n'
                  'measure frame: present 3 submit-through 11 start 10000000 end 30000000 cpu 1 tid 1\n')
        delayed = 'measure gpu: submit 11 present 0 slot 1 part 0 ready 1 duration 4000000\n'
        r = m.detailed(frames,frames+delayed)
        self.assertEqual(r['gpu_correlated_presentations'][0]['gpu_ms'],4)
        self.assertIsNone(m.detailed(frames)['gpu_correlated_presentations'][0]['gpu_ms'])

    def test_overlaps_not_added_to_frame_and_cap_explicit(self):
        text = ('measure frame: present 2 submit-through 1 start 100 end 100000100 cpu 1 tid 1\n'
                'measure event: asset-read id 7 submit 0 present 0 epoch 1 start 100 end 1000100 value 4096 tid 2\n'
                'measure: truncated limit 2')
        r=m.detailed(text)
        self.assertTrue(r['truncated'])
        self.assertEqual(r['events']['asset-read']['value_total'],4096)
        self.assertEqual(r['largest_hitches'][0]['overlapping_events_ms']['asset-read'],1)


    def test_partial_gpu_submission_is_unknown(self):
        text = ('measure frame: present 2 submit-through 10 start 0 end 10000000 cpu 1 tid 1\n'
                'measure frame: present 3 submit-through 11 start 10000000 end 30000000 cpu 1 tid 1\n'
                'measure gpu: submit 11 present 0 slot 1 part 0 ready 1 duration 4000000\n'
                'measure gpu: submit 11 present 0 slot 1 part 1 ready 0 duration 0\n')
        r=m.detailed(text)
        self.assertIsNone(r['gpu_correlated_presentations'][0]['gpu_ms'])
        self.assertEqual(r['unavailable_gpu_parts'],1)


if __name__ == '__main__':
    unittest.main()
