#!/usr/bin/env python3
"""Normalize registered landscape layers and package compact foreground halves."""
import argparse
from pathlib import Path
from PIL import Image
from generate_lvgl_argb8888 import write_c_image

p=argparse.ArgumentParser()
p.add_argument('base',type=Path); p.add_argument('grass',type=Path); p.add_argument('flowers',type=Path)
p.add_argument('output',type=Path); p.add_argument('c_output',type=Path)
a=p.parse_args(); a.output.mkdir(parents=True,exist_ok=True)
includes=[]
for label,source in [('base',a.base),('grass',a.grass),('flowers',a.flowers)]:
    im=Image.open(source).convert('RGBA').resize((410,502),Image.Resampling.LANCZOS)
    path=a.output/f'{label}.png'; im.save(path)
    if label=='base':
        name='pet_scene_base'; write_c_image(path,a.c_output.parent/f'{name}.c',name); includes.append(f'#include "{name}.c"')
    else:
        for half in range(2):
            crop=im.crop((half*205,340,(half+1)*205,502))
            path=a.output/f'{label}_{half}.png'; crop.save(path)
            name=f'pet_scene_{label}_{half}'
            write_c_image(path,a.c_output.parent/f'{name}.c',name); includes.append(f'#include "{name}.c"')
a.c_output.write_text('\n'.join(includes)+'\n')
