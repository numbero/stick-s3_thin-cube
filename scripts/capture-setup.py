#!/usr/bin/env python3
"""Capture actual StickS3 setup pixels over USB; requires pyserial and Pillow."""
import argparse
from array import array
from pathlib import Path
import sys
import time
import serial
from PIL import Image

parser=argparse.ArgumentParser()
parser.add_argument("port")
parser.add_argument("output",type=Path)
args=parser.parse_args()
port=serial.Serial()
port.port,port.baudrate,port.timeout=args.port,115200,.25
port.dtr=port.rts=False
port.open()
messages=[]
try:
    deadline=time.monotonic()+12
    while time.monotonic()<deadline:
        line=port.readline()
        messages.append(line.decode(errors="replace"))
        if b"[prov-ui]" in line or b"[hb]" in line:
            break
    port.write(b"screen\n")
    deadline=time.monotonic()+15
    while time.monotonic()<deadline:
        line=port.readline()
        messages.append(line.decode(errors="replace"))
        header=line.strip().split()
        if len(header)==5 and header[0]==b"TCFB":
            break
    else:
        raise SystemExit("No setup screenshot header received")
    _,width,height,format_name,count=header
    width,height,count=int(width),int(height),int(count)
    if format_name!=b"RGB565LE" or (width,height,count)!=(135,240,64800):
        raise SystemExit("Unexpected framebuffer format")
    data=bytearray()
    while len(data)<count and time.monotonic()<deadline:
        data.extend(port.read(count-len(data)))
    if len(data)!=count or port.readline().strip()!=b"" or port.readline().strip()!=b"TCFB-END":
        raise SystemExit("Incomplete framebuffer or interleaved serial data")
    pixels=array("H",data)
    if sys.byteorder!="little":
        pixels.byteswap()
    rgb=[(((p>>11)&31)*255//31,((p>>5)&63)*255//63,(p&31)*255//31) for p in pixels]
    image=Image.new("RGB",(width,height))
    image.putdata(rgb)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    image.save(args.output)
    args.output.with_suffix(".log").write_text("".join(messages))
    print(f"Captured actual device framebuffer: {args.output} ({width}x{height})")
finally:
    port.close()
