#!/usr/bin/env python3
"""Pack two generated clouds, preserving their soft alpha and painted shading."""
import argparse
from pathlib import Path
from PIL import Image
from generate_lvgl_argb8888 import write_c_image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('c_output', type=Path)
    parser.add_argument('--split', type=float, default=0.5,
                        help='Observed atlas gutter position as a width fraction')
    args = parser.parse_args()
    sheet = Image.open(args.source).convert('RGBA')
    args.output.mkdir(parents=True, exist_ok=True)
    includes = []
    for index, (label, width, height) in enumerate([('near',148,72), ('far',76,36)]):
        divider = round(sheet.width*args.split)
        cell = sheet.crop((0 if index==0 else divider,0,divider if index==0 else sheet.width,sheet.height))
        bounds = cell.getchannel('A').point(lambda a:255 if a>8 else 0).getbbox()
        if bounds is None:
            raise ValueError(f'Empty {label} cloud')
        subject = cell.crop(bounds)
        scale = min((width-4)/subject.width, (height-4)/subject.height)
        subject = subject.resize((round(subject.width*scale), round(subject.height*scale)), Image.Resampling.LANCZOS)
        canvas = Image.new('RGBA',(width,height))
        canvas.alpha_composite(subject,((width-subject.width)//2,(height-subject.height)//2))
        path = args.output / f'{label}.png'
        canvas.save(path)
        name = f'pet_scene_cloud_{label}'
        write_c_image(path, args.c_output.parent / f'{name}.c', name)
        includes.append(f'#include "{name}.c"')
    args.c_output.write_text('\n'.join(includes)+'\n')


if __name__ == '__main__':
    main()
