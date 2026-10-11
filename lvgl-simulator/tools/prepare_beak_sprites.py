#!/usr/bin/env python3
"""Package illustrated closed/part-open/open beaks, registered by upper bill."""
import argparse
from pathlib import Path
from PIL import Image, ImageOps
from prepare_state_sprites import clean_alpha, keep_largest_component
from generate_lvgl_argb8888 import write_c_image


def main():
    p = argparse.ArgumentParser()
    p.add_argument('source',type=Path)
    p.add_argument('output',type=Path)
    p.add_argument('c_output',type=Path)
    args = p.parse_args()
    sheet = Image.open(args.source).convert('RGBA')
    includes = []
    args.output.mkdir(parents=True,exist_ok=True)
    for view in range(3):
        cells = []
        for stage in range(3):
            cell = sheet.crop((stage*sheet.width//3,view*sheet.height//3,
                               (stage+1)*sheet.width//3,(view+1)*sheet.height//3))
            cell = keep_largest_component(clean_alpha(cell))
            bounds = cell.getchannel('A').getbbox()
            if bounds is None: raise ValueError(f'Empty beak {view}/{stage}')
            cells.append(cell.crop(bounds))
        scale = min(24/max(c.width for c in cells),27/max(c.height for c in cells))
        for stage,cell in enumerate(cells):
            cell = cell.resize((round(cell.width*scale),round(cell.height*scale)),Image.Resampling.LANCZOS)
            canvas = Image.new('RGBA',(32,36))
            # Upper bill stays registered; lower bill can extend down when open.
            x = 4 if view==2 else (32-cell.width)//2
            canvas.alpha_composite(cell,(x,5))
            for direction,frame in [('right',canvas),('left',ImageOps.mirror(canvas))]:
                name = f'pet_beak_{view}_{stage}_{direction}'
                png = args.output/f'{view}_{stage}_{direction}.png'
                frame.save(png)
                write_c_image(png,args.c_output.parent/f'{name}.c',name)
                includes.append(f'#include "{name}.c"')
    args.c_output.write_text('\n'.join(includes)+'\n')


if __name__=='__main__': main()
