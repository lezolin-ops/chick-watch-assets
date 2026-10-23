"""Verify capture parsing and per-sample agreement, including a sampling gap."""
import ctypes
import math
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='mini-capture-test-') as tmp:
    tmp = Path(tmp)
    shared = tmp/'steps.so'
    subprocess.run(['cc','-shared','-fPIC','-I'+str(root/'firmware/main'),
                    str(root/'firmware/main/watch_step_algorithm.c'),
                    str(root/'firmware/main/third_party/bangle_stepcount/stepcount.c'),
                    '-o',str(shared)],check=True)
    lib = ctypes.CDLL(str(shared))
    sample = lib.watch_step_algorithm_sample
    sample.argtypes = [ctypes.c_int16]*3+[ctypes.c_uint32,ctypes.c_int64,ctypes.POINTER(ctypes.c_bool)]
    sample.restype = ctypes.c_uint
    lib.watch_step_algorithm_reset()
    rows = []
    for i in range(800):
        now = i*80000+(400000 if i>=400 else 0)
        z = 16384+int(5000*math.sin(i*.08*2*math.pi*2))
        gap = ctypes.c_bool()
        delta = sample(0,0,z,i,now,ctypes.byref(gap))
        rows.append(f'I (0) watch_steps: STEP_SAMPLE,{now},{i},0,0,{z},{delta},{int(gap.value)}\n')
    log = tmp/'capture.log'
    log.write_text('unrelated startup log\n'+''.join(rows))
    cmd = ['python3',str(root/'firmware/tools/replay_steps.py'),str(log),'--format','capture']
    subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
    # A corrupted recorded delta must fail, rather than silently reporting counts.
    fields = rows[100].split(',')
    fields[-2] = str(int(fields[-2])+1)
    rows[100] = ','.join(fields)
    log.write_text(''.join(rows))
    assert subprocess.run(cmd,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode != 0
print('PASS: complete serial capture replay, gap reset, corrupted delta rejection')
