import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('tap_sampling',Path(__file__).with_name('sample-double-tap.py'))
sampling=importlib.util.module_from_spec(spec);spec.loader.exec_module(sampling)
class TapSamplingTests(unittest.TestCase):
    def test_preserves_raw_and_accepted_difference(self):
        keys=('raw','accepted','blocked','single','triple','unknown','inactive','buttons','settling','cooldown')
        before=dict.fromkeys(keys,0)|dict(config={'axis':2},revision=1,healthy=True,verified=True,enabled=True,errors=0,ms=0xfffffff0)
        after=before|dict(raw=2,accepted=1,blocked=1,ms=50)
        result=sampling.difference(before,after)
        self.assertEqual((result['raw'],result['accepted'],result['elapsedMs']),(2,1,66))
        for change in (dict(revision=2),dict(healthy=False),dict(errors=1),dict(config={'axis':0})):
            with self.assertRaises(ValueError):sampling.difference(before,after|change)
        with self.assertRaises(ValueError):sampling.difference(before|dict(raw=3),after)
    def test_full_configuration_crosses_usb_queue_in_bounded_chunks(self):
        import json
        capture=sampling.Capture.__new__(sampling.Capture)
        capture.snapshots=[]
        capture.collect=lambda seconds: None
        wire=bytearray(); sizes=[]
        config={'enabled': True, 'single': True, 'doubleTap': True, 'triple': True, 'filtered': False,
                'manual': True, 'wait': True, 'axis': 2, 'sensitivity': 0, 'windowMs': 400,
                'quietMs': 400, 'settleMs': 500, 'buttonGuardMs': 500, 'cooldownMs': 600}
        class Port:
            def write(self, value):
                sizes.append(len(value)); wire.extend(value)
                if wire.endswith(b'\n'):
                    body=json.loads(wire[len(b'tap apply '):])
                    capture.snapshots.append({'requestId':body['requestId'],'result':'ok','verified':True,
                                              'config':body['config'],'enabled':body['config']['enabled']})
        capture.port=Port()
        self.assertEqual(capture.configure(config)['config'],config)
        self.assertGreater(len(sizes),1)
        self.assertLessEqual(max(sizes),48)

    def test_snapshot_does_not_confuse_periodic_text_with_json(self):
        self.assertIsNone(sampling.parse_snapshot('[tap-status] raw=1'))
        self.assertEqual(sampling.parse_snapshot('[tap-json] {"supported":true,"raw":2}')['raw'],2)
        with self.assertRaises(ValueError):sampling.parse_snapshot('[tap-json] {"supported":false}')
if __name__=='__main__':unittest.main()
