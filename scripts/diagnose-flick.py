#!/usr/bin/env python3
"""Keep one USB connection for human-labelled double-flick diagnostics.

Does not flash hardware, change settings, press keys or override voice gating.
Commands: status; begin LABEL; end [REPORTED_ATTEMPTS]; sleepcheck; quit.
Opening USB may reset the StickS3. Trace is limited to 60s by firmware.
"""
import argparse
import json
import threading
import time
from pathlib import Path
import serial

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port',default='/dev/cu.usbmodem1401')
parser.add_argument('--output-dir',type=Path,required=True)
args=parser.parse_args()
args.output_dir.mkdir(parents=True,exist_ok=True)
port=serial.Serial();port.port=args.port;port.baudrate=115200;port.timeout=.1
port.dtr=port.rts=False;port.open()
log=(args.output_dir/'handheld-live.log').open('w')
started=time.monotonic();phase='setup';phase_started=started;records=[];running=True

def read_loop():
    buffer=bytearray()
    while running:
        try:buffer.extend(port.read(8192))
        except serial.SerialException as error:
            print('SERIAL_DISCONNECTED '+str(error),flush=True);return
        while b'\n' in buffer:
            line,_,rest=buffer.partition(b'\n');buffer[:]=rest
            text=line.decode(errors='replace').rstrip('\r')
            stamp=time.monotonic()-started
            log.write(f'{stamp:.3f} phase={phase} {text}\n');log.flush()
            if text.startswith('[flick-'):
                tag=text.split(']',1)[0][1:]
                fields=dict(word.split('=',1) for word in text.split() if '=' in word)
                records.append(dict(tag=tag,phase=phase,host_seconds=stamp,**fields))
                if tag not in ('flick-sample','flick-status'):print(text,flush=True)
            elif text.startswith(('[sleep-check]','[sleep-diag]','[imu-service]')):
                print(text,flush=True)

thread=threading.Thread(target=read_loop);thread.start()
def status():
    port.write(b'flick status\ntap status\n');time.sleep(.5)
    statuses=[r for r in records if r['tag']=='flick-status']
    print('STATUS '+json.dumps(statuses[-1:] or ['waiting for firmware']),flush=True)

try:
    time.sleep(12);status()
    print('READY; waiting for begin LABEL; physical voice mode is required',flush=True)
    while True:
        try:words=input().strip().split()
        except EOFError:break
        if not words:continue
        if words[0]=='status':status()
        elif words[0]=='begin' and len(words)==2:
            label=words[1]
            if not all(c.isalnum() or c in '-_' for c in label):
                print('Use letters, digits, hyphen or underscore for LABEL',flush=True);continue
            phase=label;phase_started=time.monotonic()
            port.write(b'flick reset\nflick trace\n')
            print('START '+phase,flush=True)
        elif words[0]=='end':
            port.write(b'flick trace off\nflick status\n');time.sleep(.6)
            label=phase;phase='between'
            matched=[r for r in records if r['phase']==label]
            events=[r for r in matched if r['tag'] not in ('flick-sample','flick-status')]
            statuses=[r for r in matched if r['tag']=='flick-status']
            result=dict(phase=label,duration_seconds=time.monotonic()-phase_started,
                        hits=sum(r['tag']=='flick-event' for r in events),events=events,
                        traced_samples=sum(r['tag']=='flick-sample' for r in matched),
                        final_status=statuses[-1:] or None,per_attempt_labels_available=False)
            if len(words)>1:result['operator_reported_attempts']=int(words[1])
            (args.output_dir/(label+'.json')).write_text(json.dumps(result,indent=2)+'\n')
            print('RESULT '+json.dumps(result),flush=True)
        elif words[0]=='sleepcheck':port.write(b'tap sleepcheck\n');time.sleep(1);status()
        elif words[0]=='quit':break
        else:print('status | begin LABEL | end [N] | sleepcheck | quit',flush=True)
finally:
    try:port.write(b'flick trace off\n')
    except serial.SerialException:pass
    running=False;thread.join();port.close();log.close()
