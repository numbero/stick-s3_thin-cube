#!/usr/bin/env python3
"""Read-only progress watcher; uses the dedicated Mac log, never device keys."""
import argparse
import json
from pathlib import Path
import time

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("log",type=Path)
parser.add_argument("--output",type=Path,required=True)
args=parser.parse_args()
labels={1:"charging",2:"ready-to-unplug",3:"settling",4:"discharging",5:"complete",6:"aborted"}
previous=None
last_print=0
while True:
    rows=args.log.read_text().splitlines()
    last=None
    for line in reversed(rows):
        try:
            value=json.loads(line)
        except ValueError:
            continue
        if value.get("event")=="sample":last=value;break
    if last is None:
        raise SystemExit("No valid calibration sample")
    p=last["power"];c=p["cal"]
    state=(c["state"],c["phase"] if c["state"]==4 else -1)
    summary={"state":labels.get(c["state"],str(c["state"])),"run":c["run"],"boot":p["boot"],
             "elapsedSeconds":c["elapsed"]/1000,"rawMv":p["rawMv"],"filteredMv":p["filteredMv"],
             "percent":p["percent"],"actualBrightness":p["actualBrightness"],"charging":p["charging"],
             "chargeStableSeconds":c["stableMs"]/1000,"sampleAgeSeconds":round(time.time()-last["time"],1),
             "reason":c["reason"],"model":p["model"],"time":last["time"]}
    args.output.write_text(json.dumps(summary,indent=2)+"\n")
    if state!=previous or time.time()-last_print>=60:
        print(json.dumps(summary),flush=True);previous=state;last_print=time.time()
    if c["state"]==5:break
    if c["state"]==6 or summary["sampleAgeSeconds"]>20:raise SystemExit(2)
    time.sleep(2)
