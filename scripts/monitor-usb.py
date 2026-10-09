#!/usr/bin/env python3
"""Read StickS3 logs with DTR/RTS deasserted; macOS may reset on open.

Keep one monitor open across a debug session instead of reopening repeatedly.
"""
import argparse
import time
from contextlib import nullcontext
from pathlib import Path
import serial

parser = argparse.ArgumentParser()
parser.add_argument("port", help="e.g. /dev/cu.usbmodem101")
parser.add_argument("--seconds", type=float, default=15)
parser.add_argument("--output")
parser.add_argument("--capture-request",type=Path,help="Optional file containing a PNG path; capture without reopening/resetting USB")
args = parser.parse_args()
port = serial.Serial()
port.port, port.baudrate, port.timeout = args.port, 115200, .25
port.dtr = False
port.rts = False
port.open()
def capture_frame(log):
    from PIL import Image
    import struct
    destination=Path(args.capture_request.read_text().strip())
    args.capture_request.unlink()
    port.write(b"screen\n")
    deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        line=port.readline()
        header=line.strip().split()
        if len(header)==5 and header[0]==b"TCFB":
            break
        text=line.decode(errors="replace")
        print(text,end="",flush=True)
        if log:log.write(text);log.flush()
        if b"TCFB unavailable" in line:
            print("Capture refused: device is not in an idle, stable capture layout",flush=True)
            return
    else:
        print("Capture timed out before header",flush=True);return
    _,width,height,format_name,count=header
    width,height,count=int(width),int(height),int(count)
    if format_name!=b"RGB565LE" or (width,height) not in [(135,240),(240,135)] or count!=64800:
        raise ValueError("Unexpected framebuffer header")
    data=bytearray()
    while len(data)<count and time.monotonic()<deadline:
        data.extend(port.read(count-len(data)))
    if len(data)!=count or port.readline().strip()!=b"" or port.readline().strip()!=b"TCFB-END":
        print("Capture incomplete",flush=True);return
    pixels=[(((p>>11)&31)*255//31,((p>>5)&63)*255//63,(p&31)*255//31) for (p,) in struct.iter_unpack("<H",data)]
    picture=Image.new("RGB",(width,height));picture.putdata(pixels)
    destination.parent.mkdir(parents=True,exist_ok=True);picture.save(destination)
    message=f"[capture] actual framebuffer {width}x{height} saved: {destination}\n"
    print(message,end="",flush=True)
    if log:log.write(message);log.flush()
try:
    with open(args.output, "w") if args.output else nullcontext() as log:
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            if args.capture_request and args.capture_request.exists():capture_frame(log)
            text = port.read(8192).decode(errors="replace")
            if text:
                print(text, end="", flush=True)
                if log:
                    log.write(text)
                    log.flush()
except KeyboardInterrupt:
    pass
finally:
    port.close()
