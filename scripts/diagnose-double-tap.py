#!/usr/bin/env python3
"""Collect real hand-held trials; never flashes or sends host shortcuts.

Positive trials cue one deliberate double tap per window. Negative trials
measure a labeled activity. The operator must actually perform those actions;
window hit rates do not establish general accuracy beyond the recorded trial.
"""
import argparse
import json
import time
from pathlib import Path


def fields(line):
    return dict(token.split("=", 1) for token in line.split() if "=" in token)


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--label", required=True, help="grip/hand/tap location or negative activity")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--trials", type=int, help="positive: one double tap per cued window")
    group.add_argument("--negative-seconds", type=float, help="negative: perform no deliberate double taps")
    parser.add_argument("--trial-seconds", type=float, default=3)
    parser.add_argument("--voice-only", action="store_true", help="test normal voice gate instead of manual diagnostic gate")
    parser.add_argument("--axis", choices=["x", "y", "z"], default="z")
    parser.add_argument("--sensitivity", type=int, choices=range(16), default=9)
    parser.add_argument("--source", choices=["user", "internal"], default="user")
    args = parser.parse_args()
    if (args.trials is not None and args.trials < 1) or args.trial_seconds < 2 or (args.negative_seconds is not None and args.negative_seconds <= 0):
        parser.error("use positive durations, >=1 trial and >=2 seconds per window")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    log_path = args.output_dir / "serial.log"
    summary_path = args.output_dir / "summary.json"
    if log_path.exists() or summary_path.exists():
        parser.error("choose a new output directory to preserve earlier trials")
    port = serial.Serial()
    port.port, port.baudrate, port.timeout = args.port, 115200, .05
    port.dtr = port.rts = False
    events, statuses, windows = [], [], []
    buffer = bytearray()
    started = time.monotonic()
    port.open()
    try:
        with log_path.open("x") as log:
            def collect(seconds):
                end = time.monotonic() + seconds
                batch = []
                while time.monotonic() < end:
                    buffer.extend(port.read(8192))
                    while b"\n" in buffer:
                        raw, _, rest = buffer.partition(b"\n")
                        buffer[:] = rest
                        line = raw.decode(errors="replace").rstrip("\r")
                        received = time.monotonic() - started
                        log.write(f"{received:.3f} {line}\n")
                        if line.startswith("[tap-event]"):
                            event = fields(line) | {"host_seconds": received}
                            batch.append(event)
                            events.append(event)
                            print(line, flush=True)
                        if line.startswith("[tap-status]"):
                            statuses.append(fields(line))
                    log.flush()
                return batch

            print("Waiting for boot/reconnection; opening USB may reset the device.", flush=True)
            collect(12)
            port.write(f"tap axis {args.axis}\ntap sens {args.sensitivity}\ntap {args.source}\ntap {'auto' if args.voice_only else 'on'}\ntap reset\ntap dump\ntap status\n".encode())
            statuses.clear()
            collect(2)
            if not statuses or statuses[-1].get("backend") != "legacy" or statuses[-1].get("enabled") != "1" or statuses[-1].get("healthy") != "1":
                raise RuntimeError("Legacy tap is not enabled and healthy; inspect serial.log")
            if any(statuses[-1].get(key) != str(value) for key, value in {
                "axis": "xyz".index(args.axis), "sensitivity": args.sensitivity, "source": args.source
            }.items()):
                raise RuntimeError("Tap parameters do not match the request; use diag2 or newer and inspect serial.log")
            if args.trials:
                print("Each cue: double tap ONCE, then wait. Keep the same grip and tap location.", flush=True)
                for index in range(args.trials):
                    print(f"Trial {index+1}/{args.trials}: double tap now", flush=True)
                    batch = collect(args.trial_seconds)
                    windows.append({"trial": index+1, "raw": len(batch), "accepted": sum(e.get("reason") == "accepted" for e in batch)})
            else:
                print(f"Negative trial {args.label}: {args.negative_seconds:g}s; no intentional double taps", flush=True)
                batch = collect(args.negative_seconds)
                windows.append({"seconds": args.negative_seconds, "raw": len(batch), "accepted": sum(e.get("reason") == "accepted" for e in batch)})
            # Drain any final result before taking the counters, outside scored windows.
            collect(.5)
            port.write(b"tap status\n")
            collect(.3)
            summary = {"label": args.label, "mode": "voice" if args.voice_only else "manual", "axis": args.axis, "sensitivity": args.sensitivity, "source": args.source,
                       "kind": "positive" if args.trials else "negative", "windows": windows,
                       "events": events, "last_status": statuses[-1],
                       "operator_performed_actions_required": True, "physical_wake_test": "not_performed"}
            if args.trials:
                summary["raw_window_hit_rate"] = sum(w["raw"] > 0 for w in windows) / args.trials
                summary["accepted_window_hit_rate"] = sum(w["accepted"] > 0 for w in windows) / args.trials
                summary["extra_raw_events"] = sum(max(0, w["raw"]-1) for w in windows)
            else:
                summary["raw_events_per_hour"] = windows[0]["raw"] * 3600 / args.negative_seconds
                summary["accepted_events_per_hour"] = windows[0]["accepted"] * 3600 / args.negative_seconds
            summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
            print(json.dumps(summary, ensure_ascii=False, indent=2))
    finally:
        # Return to normal voice-only diagnostics; this does not affect A/B.
        if port.is_open:
            port.write(b"tap auto\n")
            port.flush()
            port.close()


if __name__ == "__main__":
    main()
