#!/usr/bin/env python3
"""Restore a validated application into BOTH OTA slots; preserve bootloader/NVS.

Run on the computer attached to USB, after entering ROM download mode. The
portable recovery bundle includes this script, known-good.bin and manifest.json.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import time

SLOTS=(0x20000,0x4A0000)
SLOT_SIZE=0x480000

def validate(data):
    if not 288<=len(data)<=SLOT_SIZE:raise ValueError('application size outside OTA slot')
    if data[0]!=0xE9 or not 1<=data[1]<=16 or struct.unpack_from('<H',data,12)[0]!=9 or struct.unpack_from('<I',data,32)[0]!=0xABCD5432:
        raise ValueError('expected an ESP32-S3 application, not bootloader/merged image')
    at=24;checksum=0xEF
    for _ in range(data[1]):
        if at+8>len(data):raise ValueError('truncated segment header')
        _,size=struct.unpack_from('<II',data,at);at+=8
        if at+size>len(data):raise ValueError('truncated segment')
        for value in data[at:at+size]:checksum^=value
        at+=size
    check=(at+16)//16*16-1
    if check>=len(data) or data[check]!=checksum:raise ValueError('image checksum mismatch')
    end=check+1
    if data[23]:
        if end+32!=len(data) or hashlib.sha256(data[:end]).digest()!=data[end:]:raise ValueError('image SHA-256 mismatch')
    elif end!=len(data):raise ValueError('unexpected trailing bytes')

def image_path(value):
    if value:return Path(value).expanduser().resolve()
    beside=Path(__file__).resolve().parent/'known-good.bin'
    if beside.exists():return beside
    return Path(__file__).resolve().parents[1]/'backups/usb-concurrency/recovery/known-good.bin'

def commands(port,image,baud):
    base=[sys.executable,'-m','esptool','--chip','esp32s3','--port',port,'--baud',str(baud),'--before','no-reset','--after','no-reset']
    pairs=[piece for offset in SLOTS for piece in (hex(offset),str(image))]
    return [base+['write-flash','--flash-mode','keep','--flash-freq','keep','--flash-size','keep']+pairs,
            base+['verify-flash']+pairs]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image');parser.add_argument('--port');parser.add_argument('--timeout',type=float,default=120)
    parser.add_argument('--baud',type=int,default=460800);parser.add_argument('--dry-run',action='store_true')
    args=parser.parse_args()
    path=image_path(args.image);data=path.read_bytes();validate(data)
    manifest=path.with_name('manifest.json')
    if not manifest.exists():raise ValueError('manifest.json is required beside the recovery image')
    expected=json.loads(manifest.read_text())
    if hashlib.sha256(data).hexdigest()!=expected['sha256'] or len(data)!=expected['bytes']:raise ValueError('recovery image does not match its manifest')
    port=args.port
    if not port and not args.dry_run:
        from serial.tools import list_ports
        print('Enter ROM mode: remove all power, hold REC + center knob, reconnect USB.')
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            ports=[p.device for p in list_ports.comports() if p.vid==0x303A and p.pid==0x1001]
            if len(ports)>1:raise ValueError('multiple ESP32 USB devices; specify --port')
            if ports:port=ports[0];break
            time.sleep(.25)
        if not port:raise ValueError('no ROM USB device found before timeout')
    port=port or 'REPLACE_WITH_ROM_PORT'
    print('Recovery:',expected.get('firmware','known-good'),'SHA-256',expected['sha256'])
    print('Writing both application slots only; bootloader, partition tables, NVS and OTA metadata are preserved.')
    for cmd in commands(port,path,args.baud):
        print(shlex.join(cmd),flush=True)
        if not args.dry_run:subprocess.run(cmd,check=True)
    if not args.dry_run:print('Both slots verified. Power-cycle normally with REC and knob released.')
    return 0

if __name__=='__main__':
    try:sys.exit(main())
    except (ValueError,OSError,subprocess.CalledProcessError) as e:print('error:',e,file=sys.stderr);sys.exit(1)
