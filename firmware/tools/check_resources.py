#!/usr/bin/env python3
"""Reproducible host checks of the actual firmware in raw/packed modes."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
SIM=ROOT/'lvgl-simulator'
OUT=SIM/'build/resource-preview'
GEN=ROOT/'firmware/build/generated'


def run(command,env=None,cwd=ROOT):
    result=subprocess.run(list(map(str,command)),cwd=cwd,env=env,text=True,capture_output=True)
    if result.returncode:
        print(result.stdout);print(result.stderr,file=sys.stderr)
        raise RuntimeError(f'Command failed: {command[0]}')
    return result.stdout


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--skip-frames',action='store_true',help='Check resources/cache only, omit visual A/B sweep')
    args=parser.parse_args()
    cc=shutil.which('cc');cxx=shutil.which('c++')
    if not cc or not cxx: raise RuntimeError('Host C/C++ compilers required')
    for path in [SIM/'build/lvgl/liblvgl.a',SIM/'build/lvgl/liblvgl_thorvg.a']:
        if not path.exists(): raise RuntimeError('Build the LVGL simulator libraries first')
    print(run([sys.executable,ROOT/'firmware/tools/pack_resources.py']),end='')
    common=['-DLV_CONF_INCLUDE_SIMPLE','-DLV_KCONFIG_IGNORE','-DLV_LVGL_H_INCLUDE_SIMPLE',
        '-Wno-deprecated-declarations','-I'+str(SIM),'-I'+str(SIM/'lvgl'),
        '-I'+str(SIM/'lvgl/include'),'-I'+str(GEN),'-I'+str(ROOT/'tools/sdl2-prefix/include/SDL2')]
    libraries=[SIM/'build/lvgl/liblvgl.a',SIM/'build/lvgl/liblvgl_thorvg.a']
    def compile_test(source,name,flags):
        obj=SIM/'build'/f'{name}.o'; binary=SIM/'build'/name
        run([cc,*common,*flags,'-c',source,'-o',obj])
        run([cxx,obj,*libraries,'-L'+str(ROOT/'tools/sdl2-prefix/lib'),'-lSDL2',
             '-Wl,-rpath,'+str(ROOT/'tools/sdl2-prefix/lib'),'-lm','-lpthread','-o',binary])
        return binary
    cache=compile_test(ROOT/'firmware/tools/verify_resource_cache.c','verify_resource_cache',['-DPET_USE_PACKED_ASSETS=1'])
    cache_log=run([cache]);print(cache_log,end='')
    report={'cache_test':cache_log,'frames_compared':0,'different_frames':0}
    if not args.skip_frames:
        import os
        logs={}
        for label,packed in [('raw',0),('packed',1)]:
            directory=OUT/label;directory.mkdir(parents=True,exist_ok=True)
            # Prevent stale frames from a previous, larger test sweep from counting.
            for path in directory.glob('*.ppm'): path.unlink()
            binary=compile_test(SIM/'tools/verify_daily_animation.c','verify_firmware_'+label,
                [f'-DPET_USE_PACKED_ASSETS={packed}',f'-DPET_TEST_SOURCE="{ROOT}/firmware/main/pet.c"'])
            env=dict(os.environ);env['PET_PREVIEW_DIR']=str(directory)
            logs[label]=run([binary],env,cwd=SIM)
            print(f'PASS: {label} firmware complete animation regression')
        raw={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (OUT/'raw').glob('*.ppm')}
        packed={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (OUT/'packed').glob('*.ppm')}
        if not raw or raw!=packed: raise RuntimeError('Raw/packed rendered frames differ')
        report.update({'frames_compared':len(raw),'different_frames':0,'animation_logs':logs})
        print(f'PASS: {len(raw)} rendered frames are byte-for-byte identical')
    GEN.mkdir(parents=True,exist_ok=True)
    (GEN/'resource_check_report.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__': main()
