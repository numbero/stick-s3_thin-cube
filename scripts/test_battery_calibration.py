"""Independent model/tool regressions using explicitly synthetic run fixtures."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("battery_fit", Path(__file__).with_name("calibrate-battery.py"))
fit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fit)


def voltage(points, percent):
    for (a, pa), (b, pb) in zip(points, points[1:]):
        if percent <= pb:
            return round(a + (percent-pa) * (b-a) / (pb-pa))
    return points[-1][0]


def fixture(run=1, duration=1780000, model="linear"):
    curves = {0: [[3300,0],[3550,10],[3700,20],[3790,40],[3870,70],[4080,100]],
              1: [[3300,0],[3520,10],[3670,20],[3760,40],[3840,70],[4050,100]]}
    rows = []
    def row(elapsed, state, source, phase, phase_ms, mv, display, reason=0):
        return {"event":"sample", "transport":"ble", "streaming":False, "soundPlaying":False,
                "power":{"chip":"6046e5b1e87c", "boot":run+10, "ms":200000+elapsed,
                         "rawMv":mv, "medianMv":mv, "filteredMv":mv, "percent":display,
                         "source":source, "charging":False, "chargeKnown":True, "failures":0,
                         "actualBrightness":12 if phase==0 else 134, "model":model,
                         "cal":{"run":run, "state":state, "elapsed":elapsed,
                                "phase":phase,"phaseMs":phase_ms,"stableMs":180000,"reason":reason}}}
    rows.append(row(0,2,1,0,0,4100,100))
    for elapsed in range(0,duration,1000):
        phase=(elapsed//60000)%2
        reference=100*(1-elapsed/duration)
        mv=voltage(curves[phase],reference)
        display=max(0,min(100,(mv-3300)//8)) if model=="linear" else round(reference)
        rows.append(row(elapsed,4,0,phase,elapsed%60000,mv,display))
    rows.append(row(duration,5,0,(duration//60000)%2,duration%60000,3300,0,8))
    return rows


class BatteryCalibrationToolTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.root=Path(self.temp.name)
    def tearDown(self):
        self.temp.cleanup()
    def load(self, rows):
        path=self.root/"synthetic.jsonl"
        path.write_text("".join(json.dumps(r)+"\n" for r in rows))
        return fit.load_run(path)
    def model(self, run):
        return {"chip":run["chip"],"trainingBoot":run["boot"],"trainingRun":run["run"],
                "model":"csynthetic", "reference":"synthetic runtime fixture only",
                "profiles":{k:fit.fit_profile(v) for k,v in run["samples"].items()}}
    def testNonlinearModelImprovesOnIndependentRun(self):
        training=self.load(fixture())
        model=self.model(training)
        other=self.load(fixture(run=2,duration=1900000,model=model["model"]))
        result=fit.validate(model,other)
        self.assertTrue(result["passed"])
        for profile in result["profiles"].values():
            self.assertLess(profile["model"]["rmse"],profile["linearBaseline"]["rmse"]*.8)
        out=self.root/"generated.h";fit.write_header(model,out)
        self.assertIn("0x6046e5b1e87cULL",out.read_text())
    def testIncompleteDisconnectMixedAndWrongLoadRejected(self):
        for mutate in [lambda r:r[:-1],lambda r:r+[{"event":"disconnected"}],
                       lambda r:[dict(x,transport="wifi") for x in r]]:
            with self.assertRaises(ValueError):self.load(mutate(fixture()))
        rows=fixture();rows[300]["power"]["chip"]="abcdef123456"
        with self.assertRaises(ValueError):self.load(rows)
        rows=fixture();rows[300]["power"]["actualBrightness"]=0
        with self.assertRaises(ValueError):self.load(rows)
    def testDuplicateMissingAndReadFailureRejected(self):
        rows=fixture();rows.insert(100,rows[99])
        with self.assertRaises(ValueError):self.load(rows)
        rows=fixture();del rows[100:110]
        with self.assertRaises(ValueError):self.load(rows)
        rows=fixture();rows[100]["power"]["failures"]=1
        with self.assertRaises(ValueError):self.load(rows)
    def testIndependentIdentityAndActiveModelRequired(self):
        training=self.load(fixture());model=self.model(training)
        with self.assertRaises(ValueError):fit.validate(model,training)
        with self.assertRaises(ValueError):fit.validate(model,self.load(fixture(run=2)))
    def testActualDisplayMustAlsoImprove(self):
        model=self.model(self.load(fixture()))
        rows=fixture(run=2,duration=1900000,model=model["model"])
        for row in rows:
            p=row["power"];p["percent"]=max(0,min(100,(p["filteredMv"]-3300)//8))
        result=fit.validate(model,self.load(rows))
        self.assertFalse(result["passed"])


if __name__=="__main__":
    unittest.main()
