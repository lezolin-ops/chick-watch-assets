#!/usr/bin/env python3
"""Capture only battery records; reset to trigger persisted-history export."""
import argparse,csv,time
from pathlib import Path
FIELDS='seq epoch_s uptime_s battery_mv percent flags screen brightness cpu_clients awake_clients network_clients sleep_path_percent connect_attempts'.split()
def parse(text):
    result=None; rows=None
    for line in text.splitlines():
        if line.startswith('BATLOG_BEGIN,'): rows={}
        elif line.startswith('BATLOG,') and rows is not None:
            values=line.split(',')[1:]
            if len(values)==11: values+=['255','255']
            if len(values)!=len(FIELDS): raise ValueError('Invalid record length')
            record=dict(zip(FIELDS,map(int,values)));rows[record['seq']]=record
        elif line.startswith('BATLOG_END') and rows is not None:
            result=[rows[k] for k in sorted(rows)];rows=None
    if result is None: raise ValueError('No complete battery export received')
    return result
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--port',default='/dev/cu.usbmodem1101');p.add_argument('--input',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    if args.input: data=args.input.read_text(errors='replace')
    else:
        import serial
        with serial.Serial(args.port,115200,timeout=.2) as device:
            device.setDTR(False);device.setRTS(True);time.sleep(.15);device.setRTS(False)
            data='';deadline=time.monotonic()+45
            while time.monotonic()<deadline:
                data+=device.read(4096).decode(errors='replace')
                if 'BATLOG_END' in data: break
    rows=parse(data);args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=FIELDS);w.writeheader();w.writerows(rows)
    print(f'Exported {len(rows)} records to {args.output}')
