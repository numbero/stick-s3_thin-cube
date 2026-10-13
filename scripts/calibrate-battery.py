#!/usr/bin/env python3
"""Fit and independently validate a per-device, per-display-load battery curve.

Reference: remaining runtime in the fixed alternating BLE calibration protocol.
No battery current is available, so this tool never reports measured mAh/SOC.
Incomplete, interrupted, mixed-device and stale runs cannot create a model.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics

CHARGE_STABLE_MS = 180000
END_MV = 3300
PHASE_MS = 60000
SETTLED_MS = 30000
PROFILES = {0: ("dim", 12), 1: ("awake", 134)}


def load_run(path):
    rows = [json.loads(line) for line in Path(path).read_text().splitlines() if line.strip()]
    if any(row.get("event") != "sample" for row in rows):
        raise ValueError("run contains a disconnect or non-sample event")
    if not rows:
        raise ValueError("empty run")
    powers = [row["power"] for row in rows]
    identities = {(p["chip"], p["boot"], p["cal"]["run"]) for p in powers}
    if len(identities) != 1 or not powers[0]["cal"]["run"]:
        raise ValueError("mixed device/boot/run identities")
    chip, boot, run = identities.pop()
    if len(chip) != 12 or any(c not in "0123456789abcdef" for c in chip):
        raise ValueError("invalid device identity")
    if any(p["cal"]["state"] == 6 for p in powers):
        raise ValueError("aborted run")
    ready = [p for p in powers if p["cal"]["state"] == 2 and p["source"] == 1
             and p["chargeKnown"] and not p["charging"]
             and p["cal"]["stableMs"] >= CHARGE_STABLE_MS]
    if not ready:
        raise ValueError("no observed stable charge-termination/ready state")
    terminal = [p for p in powers if p["cal"]["state"] == 5 and p["cal"]["reason"] == 8]
    if len(terminal) != 1 or terminal[0]["source"] != 0 or terminal[0]["medianMv"] > END_MV:
        raise ValueError("missing/invalid software low-voltage endpoint")
    duration = terminal[0]["cal"]["elapsed"]
    if not 600000 <= duration <= 21600000:
        raise ValueError("duration outside 10-minute to 6-hour calibration range")
    data = [row for row in rows if row["power"]["cal"]["state"] == 4]
    if not data:
        raise ValueError("no discharge samples")
    failures = powers[0]["failures"]
    if any(p["failures"] != failures for p in powers):
        raise ValueError("VBAT read failure occurred")
    seen = set()
    previous = None
    samples = {name: [] for name, _ in PROFILES.values()}
    for row in data:
        p, c = row["power"], row["power"]["cal"]
        key = p["ms"]
        if key in seen:
            raise ValueError("duplicate sample")
        seen.add(key)
        if previous is not None and not 0 < c["elapsed"] - previous <= 5000:
            raise ValueError("backwards or missing discharge interval")
        previous = c["elapsed"]
        if p["source"] != 0 or p["charging"] or row["transport"] != "ble" or row["streaming"] or row["soundPlaying"]:
            raise ValueError("uncontrolled supply/audio/transport load")
        if c["phase"] != (c["elapsed"] // PHASE_MS) % 2 or c["phaseMs"] != c["elapsed"] % PHASE_MS:
            raise ValueError("unexpected calibration phase timing")
        if c["phaseMs"] < SETTLED_MS:
            continue
        name, brightness = PROFILES[c["phase"]]
        if p["actualBrightness"] != brightness or not 2500 <= p["filteredMv"] <= 4500:
            raise ValueError("unexpected actual display load or invalid voltage")
        reference = 100 * (1 - c["elapsed"] / duration)
        samples[name].append({"elapsed": c["elapsed"], "mv": p["filteredMv"],
                              "reference": reference, "display": p["percent"], "model": p.get("model", "linear")})
    for name, values in samples.items():
        if len(values) < 150 or max(v["reference"] for v in values) - min(v["reference"] for v in values) < 70:
            raise ValueError(f"insufficient full-range {name} data")
    return {"chip": chip, "boot": boot, "run": run, "durationMs": duration,
            "sourceSha256": hashlib.sha256(Path(path).read_bytes()).hexdigest(), "samples": samples}


def isotonic_points(samples):
    bins = {}
    for sample in samples:
        bucket = round(sample["mv"] / 5) * 5
        bins.setdefault(bucket, []).append(sample)
    blocks = []
    for bucket, values in sorted(bins.items()):
        weight = len(values)
        block = [sum(v["mv"] for v in values), sum(v["reference"] for v in values), weight]
        blocks.append(block)
        while len(blocks) > 1 and blocks[-2][1] / blocks[-2][2] > blocks[-1][1] / blocks[-1][2]:
            a, b = blocks[-2:]
            blocks[-2:] = [[a[i] + b[i] for i in range(3)]]
    points = [(b[0] / b[2], b[1] / b[2]) for b in blocks]
    if len(points) < 4:
        raise ValueError("insufficient distinct monotonic voltage levels")
    return points


def inverse(points, percent):
    if percent <= points[0][1]:
        left, right = points[0], next((p for p in points[1:] if p[1] > points[0][1]), points[-1])
    elif percent >= points[-1][1]:
        right = points[-1]
        left = next((p for p in reversed(points[:-1]) if p[1] < right[1]), points[0])
    else:
        left, right = next((a, b) for a, b in zip(points, points[1:]) if a[1] <= percent <= b[1])
    if right[1] == left[1]:
        raise ValueError("flat curve cannot locate percent knot")
    return left[0] + (percent - left[1]) * (right[0] - left[0]) / (right[1] - left[1])


def fit_profile(samples):
    observed = isotonic_points(samples)
    points = [[END_MV, 0]]
    for percent in range(10, 100, 10):
        # Intermediate knots require observed coverage; never label an
        # unobserved curve segment as a measured calibration point.
        if observed[0][1] <= percent <= observed[-1][1]:
            mv = round(inverse(observed, percent))
            if mv > points[-1][0]:
                points.append([mv, percent])
    top = max(round(inverse(observed, 100)), points[-1][0] + 1)
    if top > 4500 or top < 3700 or len(points) < 6:
        raise ValueError("implausible or insufficient fit")
    points.append([top, 100])
    return {"points": points, "measuredMvRange": [min(s["mv"] for s in samples), max(s["mv"] for s in samples)],
            "samples": len(samples), "zeroAnchor": "configured software endpoint 3300mV",
            "fullAnchor": "extrapolated to protocol start; verify independently"}


def level(points, mv):
    if mv <= points[0][0]:
        return 0
    if mv >= points[-1][0]:
        return 100
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        if mv <= x1:
            return y0 + ((mv - x0) * (y1 - y0) + (x1 - x0) // 2) // (x1 - x0)
    raise ValueError("invalid curve")


def metrics(errors):
    values = sorted(abs(e) for e in errors)
    return {"count": len(errors), "mae": statistics.mean(values),
            "rmse": math.sqrt(statistics.mean(e * e for e in errors)),
            "p95Absolute": values[min(len(values)-1, math.ceil(.95 * len(values))-1)], "maxAbsolute": values[-1]}


def validate(model, run):
    if model["chip"] != run["chip"] or (model["trainingBoot"], model["trainingRun"]) == (run["boot"], run["run"]):
        raise ValueError("validation must use another completed run on the same device")
    reports = {}
    for name, samples in run["samples"].items():
        if any(s["model"] != model["model"] for s in samples):
            raise ValueError("candidate model was not active throughout validation")
        points = model["profiles"][name]["points"]
        predicted = metrics([level(points, s["mv"]) - s["reference"] for s in samples])
        baseline = metrics([max(0, min(100, (s["mv"]-3300)//8)) - s["reference"] for s in samples])
        shown = [s for s in samples if s["display"] >= 0]
        if len(shown) < 150:
            raise ValueError("insufficient actual display observations")
        display = metrics([s["display"] - s["reference"] for s in shown])
        low = [s for s in samples if s["reference"] <= 20]
        low_error = max(abs(level(points, s["mv"]) - s["reference"]) for s in low)
        display_low_error = max(abs(s["display"] - s["reference"]) for s in low if s["display"] >= 0)
        passed = predicted["rmse"] <= .8 * baseline["rmse"] and predicted["mae"] <= 8 and predicted["p95Absolute"] <= 12 and low_error <= 12 and display["rmse"] <= .8 * baseline["rmse"] and display["p95Absolute"] <= 13 and display_low_error <= 13
        reports[name] = {"model": predicted, "linearBaseline": baseline, "actualDisplay": display,
                         "lowRangeMaxError": low_error, "displayLowRangeMaxError": display_low_error, "passed": passed}
    return {"passed": all(r["passed"] for r in reports.values()), "reference": model["reference"],
            "validationBoot": run["boot"], "validationRun": run["run"], "durationMs": run["durationMs"],
            "sourceSha256": run["sourceSha256"], "profiles": reports}


def write_header(model, path):
    text = ["#pragma once", '#include "battery_curve.h"', "",
            "// Per-device usable-runtime estimate; independently validate this model.",
            "// Zero endpoint is configured; 100% endpoint is extrapolated, not SOC truth.",
            "namespace CalibratedBattery {", f'static constexpr uint64_t Device = 0x{model["chip"]}ULL;',
            "static constexpr bool Enabled = true;", f'static constexpr const char* Model = "{model["model"]}";']
    for name in ["dim", "awake"]:
        title = name.capitalize()
        values = ",".join("{%d,%d}" % tuple(p) for p in model["profiles"][name]["points"])
        text += [f"static const BatteryCurvePoint {title}Points[] = {{{values}}};",
                 f"static const BatteryCurve {title}{{{title}Points,sizeof({title}Points)/sizeof({title}Points[0])}};"]
    text += ["}", ""]
    Path(path).write_text("\n".join(text))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    train = sub.add_parser("fit")
    train.add_argument("log", type=Path); train.add_argument("--output", type=Path, required=True)
    train.add_argument("--header", type=Path, required=True)
    test = sub.add_parser("validate")
    test.add_argument("log", type=Path); test.add_argument("--model", type=Path, required=True)
    test.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run = load_run(args.log)
    if args.command == "fit":
        model = {"chip": run["chip"], "trainingBoot": run["boot"], "trainingRun": run["run"],
                 "durationMs": run["durationMs"], "sourceSha256": run["sourceSha256"],
                 "reference": "remaining runtime of fixed BLE alternating 12/134 backlight, silent audio protocol; not measured SOC/mAh",
                 "validation": "pending independent run", "profiles": {k: fit_profile(v) for k, v in run["samples"].items()}}
        model["model"] = "c" + hashlib.sha256(json.dumps(model, sort_keys=True).encode()).hexdigest()[:12]
        args.output.write_text(json.dumps(model, indent=2) + "\n")
        write_header(model, args.header)
        print(json.dumps(model, indent=2))
    else:
        result = validate(json.loads(args.model.read_text()), run)
        args.output.write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
        if not result["passed"]:
            raise SystemExit(2)


if __name__ == "__main__":
    main()
