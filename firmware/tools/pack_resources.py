#!/usr/bin/env python3
"""Losslessly pack existing C image assets; originals remain development inputs.

Only Python's standard library and a host C compiler are required. The codec
comes from the already-pinned simulator LVGL checkout, with its license intact.
Every block is decompressed and byte-compared before an output is accepted.
"""
import argparse
import binascii
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]


def load_codec(output):
    codec=ROOT/'lvgl-simulator/lvgl/src/libs/lz4/lz4.c'
    glue=Path(__file__).with_name('lz4_host_memory.c')
    library=output/('host_lz4.dll' if sys.platform=='win32' else 'host_lz4.so')
    config=ROOT/'lvgl-simulator/lv_conf.h'
    if not library.exists() or library.stat().st_mtime<max(p.stat().st_mtime for p in [codec,glue,config]):
        compiler=shutil.which('cc') or shutil.which('clang') or shutil.which('gcc')
        if not compiler: raise RuntimeError('A host C compiler is required to build the bundled LZ4 codec')
        subprocess.run([compiler,'-O2','-fPIC','-shared','-DLV_CONF_INCLUDE_SIMPLE',
            '-I'+str(ROOT/'lvgl-simulator'),'-I'+str(ROOT/'lvgl-simulator/lvgl'),
            '-I'+str(ROOT/'lvgl-simulator/lvgl/include'),str(codec),str(glue),'-o',str(library)],check=True)
    lib=ctypes.CDLL(str(library))
    lib.LZ4_compressBound.argtypes=[ctypes.c_int];lib.LZ4_compressBound.restype=ctypes.c_int
    lib.LZ4_compress_default.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int]
    lib.LZ4_compress_default.restype=ctypes.c_int
    lib.LZ4_decompress_safe.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int]
    lib.LZ4_decompress_safe.restype=ctypes.c_int
    return lib


def write_if_changed(path,text):
    if not path.exists() or path.read_text()!=text:
        temporary=path.with_name(path.name+f'.tmp-{os.getpid()}')
        temporary.write_text(text)
        temporary.replace(path)  # Compiler never sees a half-written resource.


def pack(source,output):
    output.mkdir(parents=True,exist_ok=True)
    lib=load_codec(output)
    assets=[]; payloads={}; declarations=[]; payload_declarations=[]
    for path in sorted(source.glob('*.c')):
        text=path.read_text()
        match=re.search(r'const\s+lv_image_dsc_t\s+(\w+)\s*=\s*\{(.*?)\n\};',text,re.S)
        if not match or 'LV_COLOR_FORMAT_ARGB8888' not in match[2]: continue
        name=match[1]; descriptor=match.group(0)
        data_name=re.search(r'\.data\s*=\s*(\w+)',descriptor)[1]
        data=re.search(r'\b'+re.escape(data_name)+r'\s*\[[^]]*\]\s*=\s*\{(.*?)\};',text,re.S)
        if not data: raise RuntimeError(f'Cannot find pixels for {name}')
        values=re.findall(r'0x([0-9a-fA-F]{2})\b',data[1])
        if not values: raise RuntimeError(f'No BGRA byte literals in {path}')
        raw=bytes(int(v,16) for v in values)
        width=int(re.search(r'\.w\s*=\s*(\d+)',descriptor)[1])
        height=int(re.search(r'\.h\s*=\s*(\d+)',descriptor)[1])
        stride=int(re.search(r'\.stride\s*=\s*(\d+)',descriptor)[1])
        if len(raw)!=height*stride or stride!=width*4: raise RuntimeError(f'Unexpected pixel layout: {name}')
        buf=ctypes.create_string_buffer(lib.LZ4_compressBound(len(raw)))
        length=lib.LZ4_compress_default(raw,buf,len(raw),len(buf))
        if length<=0: raise RuntimeError(f'LZ4 compression failed: {name}')
        packed=buf.raw[:length]
        restored=ctypes.create_string_buffer(len(raw))
        result=lib.LZ4_decompress_safe(packed,restored,len(packed),len(raw))
        if result!=len(raw) or restored.raw!=raw: raise RuntimeError(f'Lossless roundtrip failed: {name}')
        use_compression=len(packed)+12<len(raw)*0.95
        payload=struct.pack('<III',2,len(packed),len(raw))+packed if use_compression else raw
        digest=hashlib.sha256(payload).hexdigest()
        if digest not in payloads:
            symbol=f'pet_packed_payload_{len(payloads)}';payloads[digest]=symbol
            lines=[f'static const uint8_t {symbol}[] = {{']
            lines.extend('    '+', '.join(f'0x{b:02X}' for b in payload[i:i+16])+',' for i in range(0,len(payload),16))
            lines.append('};\n');payload_declarations.append('\n'.join(lines))
        symbol=payloads[digest]
        descriptor=re.sub(r'\.data\s*=\s*\w+',f'.data = {symbol}',descriptor)
        descriptor=re.sub(r'\.data_size\s*=\s*sizeof\([^)]*\)',f'.data_size = sizeof({symbol})',descriptor)
        pose=name.startswith(('pet_daily_','pet_rest_head_','pet_sky_head_','pet_ground_head_',
                              'pet_side_body_','pet_rig_body_','pet_chick_hungry','pet_chick_sleep'))
        if use_compression:
            flags=re.search(r'\.flags\s*=\s*([^,\n}]+)',descriptor)
            if not flags: raise RuntimeError(f'Missing flags field in {name}')
            new_flags=f'({flags[1].strip()}) | LV_IMAGE_FLAGS_COMPRESSED'
            if pose: new_flags+=' | LV_IMAGE_FLAGS_USER1'
            descriptor=descriptor[:flags.start(1)]+new_flags+descriptor[flags.end(1):]
        declarations.append(descriptor)
        assets.append({'name':name,'source':str(path.relative_to(ROOT)), 'width':width,'height':height,
            'raw_bytes':len(raw),'stored_bytes':len(payload),'compressed':use_compression,
            'sha256':hashlib.sha256(raw).hexdigest(),'crc32':binascii.crc32(raw),'payload':symbol,
            'cache_policy':'pose_lru' if pose else 'resident'})
    if not assets: raise RuntimeError('No supported image assets found')
    header='/* Generated lossless resource overlay. Do not edit; regenerate from firmware/main C assets. */\n#include "lvgl.h"\n'
    write_if_changed(output/'pet_packed_assets.c',header+'\n'.join(payload_declarations+declarations)+'\n')
    # A native test can compare every decoded image with its original SHA-256.
    listing='\n'.join(f'    {{"{a["name"]}", &{a["name"]}, 0x{a["crc32"]:08X}U}},' for a in assets)
    write_if_changed(output/'pet_resource_test_entries.h',listing+'\n')
    unique_sizes={a['payload']:a['stored_bytes'] for a in assets}
    report={'format':'lossless LZ4 block / unchanged ARGB8888', 'assets':assets,
        'raw_total_bytes':sum(a['raw_bytes'] for a in assets), 'stored_total_bytes':sum(unique_sizes.values()),
        'unique_payloads':len(payloads),'roundtrip_verified':len(assets)}
    write_if_changed(output/'resource_manifest.json',json.dumps(report,indent=2)+'\n')
    print(f'PASS: {len(assets)} exact byte-for-byte roundtrips; raw {report["raw_total_bytes"]:,} B -> stored {report["stored_total_bytes"]:,} B')


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path,default=ROOT/'firmware/main')
    parser.add_argument('--output',type=Path,default=ROOT/'firmware/build/generated')
    args=parser.parse_args();pack(args.source,args.output)
