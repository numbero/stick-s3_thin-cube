#!/usr/bin/env python3
"""Measure saved-device app relaunches through the installed production app.

Uses HardwareCheck metadata only. Does not start audio, read credentials, alter
device settings, or flash firmware. Both versions use the same 200 ms readiness
poll; external file observation adds at most one 20 ms poll of uncertainty.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import plistlib
import subprocess
import time

APP = Path('/Applications/ThinCube.app')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--label', required=True)
parser.add_argument('--expected-version', required=True)
parser.add_argument('--count', type=int, default=10)
parser.add_argument('--warmup', type=int, default=1)
parser.add_argument('--timeout', type=float, default=25)
args = parser.parse_args()
if not 1 <= args.count <= 30 or not 0 <= args.warmup <= 2:
    parser.error('count must be 1..30 and warmup 0..2')
info = plistlib.loads((APP/'Contents/Info.plist').read_bytes())
if info['CFBundleShortVersionString'] != args.expected_version:
    raise SystemExit('Unexpected installed app version; refusing a mixed baseline')
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)


def running():
    return subprocess.run(['pgrep', '-u', str(os.getuid()), '-x', 'ThinCube'], capture_output=True).returncode == 0


def trial(index):
    subprocess.run(['pkill', '-TERM', '-u', str(os.getuid()), '-x', 'ThinCube'], capture_output=True)
    deadline = time.monotonic()+8
    while running() and time.monotonic() < deadline:
        time.sleep(.1)
    if running():
        raise RuntimeError('ThinCube did not exit; no second instance was started')
    time.sleep(1)
    path = output/f'run-{index:02d}.jsonl'
    if path.exists():
        raise RuntimeError(f'Refusing to overwrite {path}')
    launched = time.monotonic()
    subprocess.run(['open', str(APP), '--args', '--hardware-check', str(path)], check=True)
    began = None
    first = None
    result = None
    deadline = launched+args.timeout
    while time.monotonic() < deadline:
        if path.exists():
            for line in path.read_text().splitlines():
                try:
                    row = json.loads(line)
                except ValueError:
                    continue
                if row.get('event') == 'begin' and began is None:
                    began = time.monotonic()
                    first = row
                if row.get('event') in ('complete', 'failed'):
                    result = row
                    break
        if result is not None:
            break
        time.sleep(.02)
    ended = time.monotonic()
    success = result is not None and result.get('event') == 'complete' and result.get('connected') is True
    return {
        'index': index, 'warmup': index < args.warmup,
        'success': success, 'launchToResultMs': (ended-launched)*1000,
        'observedReadyMs': (ended-began)*1000 if success and began is not None else None,
        'alreadyConnectedAtBegin': first.get('connected', False) if first else None,
        'audioPreparedAtCompletion': result.get('audioPrepared') if result else None,
        'transport': result.get('transport') if result else None,
        'milestones': result.get('connectionTiming', []) if result else [],
        'failure': None if success else (result.get('reason', 'failed') if result else 'external_timeout'),
    }


results = []
failures = 0
try:
    for index in range(args.warmup+args.count):
        row = trial(index)
        results.append(row)
        failures = 0 if row['success'] else failures+1
        print(json.dumps({key: row[key] for key in ('index', 'warmup', 'success', 'observedReadyMs', 'audioPreparedAtCompletion', 'transport', 'failure')}), flush=True)
        if failures >= 3:
            print('Stopped after three consecutive failures; keep them in the report.', flush=True)
            break
finally:
    payload = {'label': args.label, 'appVersion': info['CFBundleShortVersionString'],
               'appBuild': info['CFBundleVersion'], 'readinessPollMs': 200, 'filePollMs': 20,
               'requestedCount': args.count, 'warmupCount': args.warmup,
               'recordedAt': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'runs': results}
    (output/'results.json').write_text(json.dumps(payload, ensure_ascii=False, indent=2)+'\n')
