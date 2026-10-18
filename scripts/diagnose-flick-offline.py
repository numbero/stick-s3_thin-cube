#!/usr/bin/env python3
"""USB start -> unplug for natural gestures -> reconnect USB and export RAM.

No wireless data access, credentials, backups or flashing. Keep DTR/RTS high
and clear HUPCL to avoid ESP32-S3 USB/JTAG reset sequences on open/close.
Commands: record LABEL; export [REPORTED_ATTEMPTS]; quit.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import re
import time


def open_port(name):
    import termios
    import serial
    port=serial.Serial();port.port=name;port.baudrate=115200;port.timeout=.1
    port.dtr=port.rts=True;port.open()
    attrs=termios.tcgetattr(port.fileno());attrs[2]&=~termios.HUPCL
    termios.tcsetattr(port.fileno(),termios.TCSANOW,attrs)
    return port


def lines(port,seconds):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        text=port.readline().decode(errors='replace').strip()
        if 'ESP-ROM:' in text or '=== clawd' in text:
            raise RuntimeError('Device reset detected; do not count this capture as a valid trial')
        if 'TCFLICK-ERROR' in text or '[flick-record-error]' in text:
            raise RuntimeError(text)
        if text:
            yield text


def report(port):
    for text in lines(port,5):
        if text.startswith('[flick-record] '):
            return dict(word.split('=',1) for word in text.split()[1:] if '=' in word)
    raise RuntimeError('No capture acknowledgement')


def parse_sample(text,index):
    words=text.split()
    if len(words)!=13 or words[0]!='TCFLICK' or int(words[1])!=index:
        raise ValueError('Missing or out-of-order USB sample')
    row=[int(words[2])]+[float(v) for v in words[3:9]]+[int(v) for v in words[9:]]
    if (not 0<=row[0]<=0xffffffff or not all(math.isfinite(v) for v in row[1:7])
            or any(not 0<=row[i]<=limit for i,limit in ((7,7),(8,6),(9,4),(10,9)))):
        raise ValueError('Invalid sample values')
    return row


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='/dev/cu.usbmodem1401')
    parser.add_argument('--output-dir',type=Path,required=True)
    args=parser.parse_args();args.output_dir.mkdir(parents=True,exist_ok=True)
    label=None;initial=None
    print('READY: record LABEL | export [N] | quit; each command closes USB without resetting',flush=True)
    while True:
        try:
            words=input().strip().split()
            if not words:continue
            if words[0]=='quit':break
            if words[0]=='record' and len(words)==2:
                if not re.fullmatch(r'[A-Za-z0-9_-]{1,60}',words[1]):raise ValueError('Use a short filename label')
                label=words[1]
                with open_port(args.port) as port:
                    port.write(b'flick record\n');initial=report(port)
                if initial.get('state')!='recording':raise RuntimeError('Capture did not start')
                (args.output_dir/(label+'.json')).write_text(json.dumps({'start':initial},indent=2)+'\n')
                print('RECORDING '+json.dumps(initial)+'; USB closed, unplug now; maximum 60 seconds',flush=True)
            elif words[0]=='export' and initial:
                with open_port(args.port) as port:
                    port.write(b'flick finish\n');frozen=report(port)
                    if frozen.get('id')!=initial['id'] or frozen.get('started_ms')!=initial['started_ms']:
                        raise RuntimeError('Capture changed or device rebooted while unplugged')
                    count=int(frozen['count'])
                    if not 0<count<=6000:raise RuntimeError('No samples retained')
                    port.write(b'flick export\n');rows=[];begun=False;complete=False
                    for text in lines(port,30):
                        if text.startswith('TCFLICK-BEGIN '):
                            if text.split()[1:]!=[initial['id'],str(count)]:raise RuntimeError('Unexpected capture header')
                            begun=True
                        elif text.startswith('TCFLICK '):
                            if not begun:raise RuntimeError('Samples without a header')
                            rows.append(parse_sample(text,len(rows)))
                            if len(rows)>count:raise RuntimeError('Export exceeds its declared length')
                        elif text.startswith('TCFLICK-END '):
                            if text.split()[1:]!=[initial['id'],str(count)] or len(rows)!=count:
                                raise RuntimeError('Incomplete USB export')
                            complete=True;break
                    if not complete:raise RuntimeError('USB export timed out; RAM data was not cleared')
                for a,b in zip(rows,rows[1:]):
                    if not 0<((b[0]-a[0])&0xffffffff)<60000:raise ValueError('Sample clock is inconsistent')
                target=args.output_dir/(label+'.csv')
                with target.open('w',newline='') as f:
                    writer=csv.writer(f);writer.writerow(['ms','ax_g','ay_g','az_g','gx_dps','gy_dps','gz_dps','flags','stage','event','reason']);writer.writerows(rows)
                result=dict(start=initial,capture=frozen,csv=target.name,downloaded_samples=len(rows),
                            valid_voice_samples=sum(r[7]==5 for r in rows),recognized_events=sum(r[9]==3 for r in rows),
                            per_attempt_labels_available=False,transport='usb')
                if len(words)>1:result['operator_reported_attempts']=int(words[1])
                (args.output_dir/(label+'.json')).write_text(json.dumps(result,indent=2)+'\n')
                print('SAVED '+str(target)+' '+json.dumps(result),flush=True)
            else:print('record LABEL | export [N] | quit',flush=True)
        except (OSError,ValueError,RuntimeError) as error:print('ERROR '+str(error),flush=True)
        except (EOFError,KeyboardInterrupt):break


if __name__=='__main__':main()
