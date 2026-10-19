#!/usr/bin/env python3
"""Automated, cued hand-held tap acquisition. No key injection or flashing.

The operator only performs the cued actions. Configuration, intervals, counter
snapshots, negative sample, restoration and JSON output are automatic. A cued
window is not ground truth unless the operator confirms the actions occurred.
"""
import argparse
import json
import subprocess
import sys
import time
import uuid
from pathlib import Path


def parse_snapshot(line):
    if not line.startswith('[tap-json] '):
        return None
    value = json.loads(line[len('[tap-json] '):])
    if not isinstance(value, dict) or value.get('supported') is not True:
        raise ValueError('Legacy tap telemetry is unavailable')
    return value


def difference(before, after):
    if (before.get('config') != after.get('config') or before.get('revision') != after.get('revision')
            or any(s.get('healthy') is not True or s.get('verified') is not True or s.get('enabled') is not True for s in (before, after))
            or before.get('errors') != after.get('errors')):
        raise ValueError('Configuration/health changed during the interval')
    elapsed = (after['ms'] - before['ms']) & 0xffffffff
    if elapsed >= 600000:
        raise ValueError('Device restarted or interval exceeds ten minutes')
    result = {'elapsedMs': elapsed}
    keys = ['raw', 'accepted', 'blocked', 'single', 'triple', 'unknown', 'inactive', 'buttons', 'settling', 'cooldown']
    if 'softwareRaw' in before: keys += ['softwareRaw', 'softwareAccepted', 'softwareBlocked', 'softwareCandidates', 'softwareRejected', 'softwareSamples']
    keys += [key for key in ('softwareMotionRejected', 'softwareSingleRejected', 'softwareMultipleRejected') if key in before]
    for key in keys:
        low, high = before[key], after[key]
        if not isinstance(low, int) or not isinstance(high, int) or high < low:
            raise ValueError('Counters reset during the interval')
        result[key] = high - low
    return result


class Capture:
    def __init__(self, port, directory):
        self.port, self.directory = port, directory
        self.buffer = bytearray()
        self.snapshots, self.events = [], []
        self.wave, self.wave_count, self.wave_complete, self.wave_metadata = [], None, False, {}
        self.wave_gyro = {}
        self.log = (directory / 'serial.log').open('x')
        self.started = time.monotonic()

    def collect(self, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.buffer.extend(self.port.read(8192))
            while b'\n' in self.buffer:
                raw, _, remaining = self.buffer.partition(b'\n'); self.buffer[:] = remaining
                line = raw.decode(errors='replace').rstrip('\r')
                at = time.monotonic() - self.started
                self.log.write(f'{at:.3f} {line}\n'); self.log.flush()
                if line.startswith('[tap-wave] '):
                    self.wave_metadata = dict(token.split('=',1) for token in line.split() if '=' in token)
                if line.startswith('TCWAVE-BEGIN '):
                    self.wave_count = int(line.split()[1]); self.wave = []; self.wave_complete = False
                elif line.startswith('TCWAVE '):
                    _, index, x, y, z = line.split()
                    values = [int(x), int(y), int(z)]
                    if int(index) != len(self.wave) or any(not -32768 <= value <= 32767 for value in values):
                        raise ValueError('Non-contiguous or invalid FIFO samples')
                    self.wave.append(values)
                elif line.startswith('TCWAVE-GYRO '):
                    import math
                    _, index, value = line.split(); value = float(value)
                    if not math.isfinite(value) or not 0 <= value <= 4000: raise ValueError('Invalid FIFO gyro sidecar')
                    self.wave_gyro[int(index)] = value
                elif line == 'TCWAVE-END':
                    self.wave_complete = len(self.wave) == self.wave_count
                snap = parse_snapshot(line)
                if snap is not None:
                    self.snapshots.append(snap)
                if line.startswith('[tap-event] '):
                    self.events.append({'hostSeconds': at, 'line': line})

    def snapshot(self):
        count = len(self.snapshots)
        self.port.write(b'tap json\n')
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            self.collect(.05)
            if len(self.snapshots) > count:
                return self.snapshots[-1]
        raise RuntimeError('USB telemetry timeout (v0.4.51 or newer is required)')

    def configure(self, config):
        request = str(uuid.uuid4())
        command = {'action': 'configure', 'requestId': request, 'config': config}
        wire = ('tap apply ' + json.dumps(command, separators=(',', ':')) + '\n').encode()
        # USB CDC has a smaller RX queue than the bounded command line. Pace
        # complete chunks while draining logs, so a long JSON line cannot overrun it.
        for offset in range(0, len(wire), 48):
            self.port.write(wire[offset:offset+48]); self.collect(.04)
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            self.collect(.05)
            if self.snapshots and self.snapshots[-1].get('requestId') == request:
                actual = self.snapshots[-1]
                if actual.get('result') != 'ok' or actual.get('verified') is not True or actual.get('config') != config or actual.get('enabled') != config['enabled']:
                    raise RuntimeError('Requested tap configuration did not pass readback: ' + actual.get('result', 'unknown'))
                return actual
        raise RuntimeError('Configuration acknowledgement timeout')

    def stage(self, name, **extra):
        value = {'stage': name, 'hostSeconds': time.monotonic()-self.started, **extra}
        (self.directory / 'progress.json').write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n')
        print(json.dumps(value, ensure_ascii=False), flush=True)


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--label', required=True)
    parser.add_argument('--trials', type=int, default=10)
    parser.add_argument('--trial-seconds', type=float, default=5)
    parser.add_argument('--negative-seconds', type=float, default=60)
    parser.add_argument('--sensitivity', type=int, choices=range(16), default=0)
    parser.add_argument('--window-ms', type=int, choices=range(250, 1001, 5), default=400)
    parser.add_argument('--start-file', type=Path, help='Wait for this file before cueing; create only when the operator is ready')
    parser.add_argument('--software-config', type=Path, help='Optional JSON overrides for software detector parameters')
    parser.add_argument('--software', action='store_true', help='Enable the 200 Hz software comparator alongside native tap')
    parser.add_argument('--wave-positive', action='store_true', help='Capture 200 Hz filtered FIFO during positive windows, then automatically export')
    parser.add_argument('--wave-negative', action='store_true', help='Capture up to 60 seconds of negative FIFO data for offline checks')
    parser.add_argument('--speak', action='store_true', help='Use the Mac speaker for automatic Chinese trial cues')
    args = parser.parse_args()
    if args.trials < 0 or args.trial_seconds < 3 or not 0 < args.negative_seconds < 600:
        parser.error('use >=0 trials, >=3 seconds per window, and a negative interval below ten minutes')
    if args.wave_negative and args.negative_seconds > 60:
        parser.error('negative FIFO capture must fit within 60 seconds')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    if (args.output_dir/'serial.log').exists():
        parser.error('use a fresh output directory')
    port = serial.Serial(); port.port = args.port; port.baudrate = 115200; port.timeout = .05; port.dtr = port.rts = False
    port.open(); capture = Capture(port, args.output_dir)
    previous = None
    summary = {'label': args.label, 'operatorConfirmationRequired': True, 'operatorConfirmed': False, 'trials': [], 'negative': None, 'completed': False}
    voices = []

    def cue(text):
        print(text, flush=True)
        if args.speak and sys.platform == 'darwin':
            process = subprocess.Popen(['/usr/bin/say', '-v', 'Tingting', text])
            voices.append(process)
            return process
        return None

    def begin_wave():
        capture.wave_complete = False; capture.wave_gyro = {}; capture.wave_metadata = {}
        low = capture.snapshot()['ms']; port.write(b'tap record\n'); capture.collect(.3)
        high = capture.snapshot()['ms']
        if capture.wave_metadata.get('active') != '1': raise RuntimeError('FIFO capture was refused')
        return [low, high]

    def export_wave(stem):
        port.write(b'tap finish\n'); capture.collect(.3)
        cue('动作采样结束。请将设备屏幕朝上放好，等待自动导出。')
        capture.stage(stem+'-export-preparation'); capture.collect(8)
        port.write(b'tap export\n'); deadline = time.monotonic()+60
        while not capture.wave_complete and time.monotonic()<deadline: capture.collect(.1)
        if not capture.wave_complete or capture.wave_metadata.get('overflow') != '0' or capture.wave_metadata.get('restored') != '1':
            raise RuntimeError('FIFO export is incomplete, overflowed, or not restored')
        import csv
        name=stem+'-wave.csv'
        with (args.output_dir/name).open('w') as output:
            writer=csv.writer(output); writer.writerow(['index','relativeMs','sensorXRaw','sensorYRaw','sensorZRaw','gyroDps'])
            for index, values in enumerate(capture.wave): writer.writerow([index,index*5,*values,capture.wave_gyro.get(index,'')])
        return {'file':name,'samples':len(capture.wave),'periodMs':5,'rangeG':8,'filtered':True,
                'metadata':capture.wave_metadata,'note':'FIFO filtered signal differs from native unfiltered input; capture is bounded to 60 seconds.'}

    try:
        capture.stage('boot'); capture.collect(10)
        first = capture.snapshot(); previous = first['config'].copy()
        config = previous | {'enabled': True, 'single': True, 'doubleTap': True, 'triple': True, 'filtered': False,
                             'axis': 2, 'sensitivity': args.sensitivity, 'windowMs': args.window_ms, 'quietMs': 400, 'wait': True,
                             'manual': True, 'settleMs': 500, 'buttonGuardMs': 500, 'cooldownMs': 600}
        if args.software: config['software'] = True
        if args.software_config:
            overrides=json.loads(args.software_config.read_text())
            if not isinstance(overrides,dict) or any(key not in config or not key.startswith('soft') for key in overrides):
                raise ValueError('Only known software parameters may be overridden')
            config.update(overrides)
        summary['configurationReadback'] = capture.configure(config)
        capture.collect(1)
        capture.stage('ready', configuration=config)
        if args.start_file:
            while not args.start_file.exists(): capture.collect(.2)
        if args.trials:
            cue('准备开始。保持正常对讲握姿。每次听到提示，只双敲一次，然后等待。')
            capture.collect(7)
        if args.wave_positive:
            if args.trials * (args.trial_seconds + (4 if args.speak else 0)) > 55:
                raise ValueError('Positive FIFO capture must fit within 55 seconds')
            summary['waveStartDeviceMsBounds'] = begin_wave()
        for index in range(args.trials):
            before = capture.snapshot()
            capture.stage('positive', trial=index+1, total=args.trials)
            process = cue(f'第{index+1}组，双敲一次。')
            # Begin the baseline before speech, but allow the complete action
            # window AFTER speech has finished. TTS startup must not steal time
            # or move a delayed first result into the next trial.
            while process is not None and process.poll() is None:
                capture.collect(.05)
            capture.collect(args.trial_seconds)
            after = capture.snapshot()
            summary['trials'].append({'trial': index+1, 'before': before, 'after': after, 'difference': difference(before, after)})
            (args.output_dir/'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2)+'\n')
        if args.wave_positive:
            summary['wave'] = export_wave('positive')
        cue(f'接下来{int(args.negative_seconds)}秒，正常拿起、放下、换握，正常短按按键。不要有意双敲。')
        capture.stage('negative-preparation'); capture.collect(9)
        if args.wave_negative: summary['negativeWaveStartDeviceMsBounds'] = begin_wave()
        before = capture.snapshot(); capture.stage('negative', seconds=args.negative_seconds)
        cue('负样本现在开始。')
        capture.collect(args.negative_seconds)
        after = capture.snapshot()
        summary['negative'] = {'before': before, 'after': after, 'difference': difference(before, after)}
        if args.wave_negative: summary['negativeWave'] = export_wave('negative')
        summary['completed'] = True
        cue('采样完成，可以停下了。数据已经自动保存，不需要操作软件。')
    except BaseException as error:
        summary['error'] = repr(error)
        raise
    finally:
        try:
            if previous is not None:
                summary['restorationReadback'] = capture.configure(previous)
        except Exception as error:
            summary['restorationError'] = repr(error)
        summary['events'] = capture.events
        (args.output_dir/'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2)+'\n')
        capture.stage('complete' if summary['completed'] else 'failed')
        for process in voices:
            if process.poll() is None: process.terminate()
        capture.log.close(); port.close()


if __name__ == '__main__': main()
