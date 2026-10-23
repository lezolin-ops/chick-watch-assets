#!/usr/bin/env python3
"""Replay Bangle upstream CSV or explicit STEP_SAMPLE serial captures.
Builds the same C implementation as the firmware, in a temporary directory.
Upstream data is 8192/g; QMI ±2g captures are 16384/g.
"""
import argparse
import ctypes
import csv
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', type=Path)
    p.add_argument('--format', choices=['upstream', 'capture'], required=True)
    p.add_argument('--expected', type=int)
    p.add_argument('--nominal-cadence', action='store_true', help='Upstream only: ignore log times, as upstream harness does')
    args = p.parse_args()
    if args.nominal_cadence and args.format != 'upstream':
        p.error('--nominal-cadence is only for upstream comparisons')
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='mini-step-replay-') as tmp:
        libpath = Path(tmp) / 'stepcount.so'
        subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-I'+str(root/'main'),
                        str(root/'main/watch_step_algorithm.c'),
                        str(root/'main/third_party/bangle_stepcount/stepcount.c'),
                        '-o', str(libpath)], check=True)
        lib = ctypes.CDLL(str(libpath))
        sample = lib.watch_step_algorithm_sample
        sample.argtypes = [ctypes.c_int16]*3+[ctypes.c_uint32, ctypes.c_int64, ctypes.POINTER(ctypes.c_bool)]
        sample.restype = ctypes.c_uint
        lib.watch_step_algorithm_reset()
        total = count = gaps = clipped = mismatches = 0
        last_time = None
        with args.input.open() as f:
            for line in f:
                if args.format == 'capture':
                    if 'STEP_SAMPLE,' not in line:
                        continue
                    row = line.split('STEP_SAMPLE,', 1)[1].strip().split(',')
                    if len(row) != 7:
                        raise ValueError('Incomplete capture row')
                    now, ts, x, y, z, recorded, reset = map(int, row)
                    if reset:
                        lib.watch_step_algorithm_reset()
                else:
                    row = next(csv.reader([line]))
                    if not row or row[0].startswith('Time'):
                        continue
                    ms, x, y, z = map(int, row[:4])
                    now, ts, recorded = (count*80000 if args.nominal_cadence else ms*1000), count & 0xffffff, None
                    xyz = [v*2 for v in (x,y,z)]
                    clipped += sum(v < -32768 or v > 32767 for v in xyz)
                    x,y,z = [max(-32768, min(32767,v)) for v in xyz]
                if last_time is not None and now <= last_time:
                    raise ValueError('Non-increasing host timestamps')
                last_time = now
                gap = ctypes.c_bool()
                delta = sample(x,y,z,ts,now,ctypes.byref(gap))
                total += delta
                count += 1
                gaps += gap.value
                if recorded is not None and recorded != delta:
                    mismatches += 1
        if not count:
            raise ValueError('No samples found')
        result = dict(samples=count, steps=total, resets_or_gaps=gaps,
                      clipped_axes=clipped, capture_delta_mismatches=mismatches)
        if args.expected is not None:
            result['reference_steps'] = args.expected
            result['difference'] = total-args.expected
            if args.expected:
                result['error_percent'] = round(100*(total-args.expected)/args.expected, 2)
        print(json.dumps(result, indent=2))
        if mismatches:
            raise SystemExit('Capture replay differs from recorded firmware deltas')


if __name__ == '__main__':
    main()
