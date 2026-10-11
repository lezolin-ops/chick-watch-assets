#!/usr/bin/env python3
"""Package complete side portraits. No runtime masking or face patching."""
import argparse
from pathlib import Path
from PIL import Image, ImageOps
from prepare_state_sprites import clean_alpha, keep_largest_component
from generate_lvgl_argb8888 import write_c_image
from prepare_blink_geometry import geometry


def main():
    p = argparse.ArgumentParser()
    p.add_argument('source',type=Path)
    p.add_argument('output',type=Path)
    p.add_argument('c_output',type=Path)
    args = p.parse_args()
    sheet = Image.open(args.source).convert('RGBA')
    cells = []
    for stage in range(3):
        cell = sheet.crop((stage*sheet.width//3,0,(stage+1)*sheet.width//3,sheet.height))
        cell = keep_largest_component(clean_alpha(cell))
        cells.append(cell.crop(cell.getchannel('A').getbbox()))
    scale = 144/max(c.height for c in cells)
    args.output.mkdir(parents=True,exist_ok=True)
    includes, eyes = [], []
    for stage,cell in enumerate(cells):
        # All portraits share one scale, top and belly baseline.
        cell = cell.resize((round(cell.width*scale),round(cell.height*scale)),Image.Resampling.LANCZOS)
        canvas = Image.new('RGBA',(192,192))
        canvas.alpha_composite(cell,((192-cell.width)//2,162-cell.height))
        for direction,frame in [('right',canvas),('left',ImageOps.mirror(canvas))]:
            name=f'pet_side_body_{stage}_{direction}'
            png=args.output/f'{stage}_{direction}.png'
            frame.save(png)
            write_c_image(png,args.c_output.parent/f'{name}.c',name)
            includes.append(f'#include "{name}.c"')
        eyes.append(geometry(args.output/f'{stage}_right.png'))
    args.c_output.write_text('\n'.join(includes)+'\n')
    (args.c_output.parent/'pet_side_body_eyes.h').write_text(
        '/* Eye positions in complete side portraits. */\nstatic const eye_pose_t side_body_eyes[3] = {\n'+
        '\n'.join('    '+eye+',' for eye in eyes)+'\n};\n')


if __name__=='__main__': main()
