import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('offline',Path(__file__).with_name('diagnose-flick-offline.py'))
offline=importlib.util.module_from_spec(spec);spec.loader.exec_module(offline)


class UsbCaptureTests(unittest.TestCase):
    def test_complete_sample(self):
        row=offline.parse_sample('TCFLICK 0 100 0 -1 0 200 -30 0 5 2 0 0',0)
        self.assertEqual(row,[100,0.,-1.,0.,200.,-30.,0.,5,2,0,0])

    def test_missing_and_corrupt_sample(self):
        for text in ('TCFLICK 1 100 0 -1 0 200 -30 0 5 2 0 0',
                     'TCFLICK 0 100 nan -1 0 200 -30 0 5 2 0 0',
                     'TCFLICK 0 100 0 -1 0 200 -30 0 8 2 0 0',
                     'TCFLICK 0 100 0 -1 0 200 -30 0 5 2 0'):
            with self.subTest(text=text):
                with self.assertRaises(ValueError):offline.parse_sample(text,0)


if __name__=='__main__':unittest.main()
