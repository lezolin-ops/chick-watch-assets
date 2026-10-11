#!/usr/bin/env python3
"""Package original-style foot artwork as independently movable bitmap parts."""
import argparse
from pathlib import Path
from PIL import Image, ImageOps
from prepare_state_sprites import clean_alpha, keep_largest_component
from generate_lvgl_argb8888 import write_c_image


def main():
    p = argparse.ArgumentParser()
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('c_output', type=Path)
    args = p.parse_args()
    sheet = Image.open(args.source).convert('RGBA')
    cells = []
    for index in range(2):
        cell = sheet.crop((index * sheet.width // 2, 0, (index + 1) * sheet.width // 2, sheet.height))
        cell = keep_largest_component(clean_alpha(cell))
        bounds = cell.getchannel('A').getbbox()
        if bounds is None:
            raise ValueError(f'Empty foot {index}')
        cells.append(cell.crop(bounds))
    # Offline sizing, shared by both feet; no runtime squash or stretching.
    # Allow the hidden attachment stem to grow without shrinking the foot sole.
    scale = min(32 / max(c.width for c in cells), 32 / max(c.height for c in cells))
    args.output.mkdir(parents=True, exist_ok=True)
    includes = []
    for index, cell in enumerate(cells):
        pixels = [(x,y) for y in range(max(1, cell.height // 4)) for x in range(cell.width)
                  if cell.getpixel((x,y))[3] > 128]
        ankle_x = sum(x for x,y in pixels) / len(pixels)
        cell = cell.resize((round(cell.width * scale), round(cell.height * scale)), Image.Resampling.LANCZOS)
        canvas = Image.new('RGBA', (48, 40))
        canvas.alpha_composite(cell, (round(18 - ankle_x * scale), 40 - cell.height))
        label = 'near' if index == 0 else 'far'
        for direction, frame in [('right', canvas), ('left', ImageOps.mirror(canvas))]:
            name = f'pet_rig_foot_{label}_{direction}'
            png = args.output / f'foot_{label}_{direction}.png'
            frame.save(png)
            write_c_image(png, args.c_output.parent / f'{name}.c', name)
            includes.append(f'#include "{name}.c"')
    for direction in ['right', 'left']:
        name = f'pet_rig_body_{direction}'
        write_c_image(args.output / f'body_{direction}.png', args.c_output.parent / f'{name}.c', name)
        includes.append(f'#include "{name}.c"')
    args.c_output.write_text('\n'.join(includes) + '\n')


if __name__ == '__main__':
    main()
