#!/usr/bin/env python3
"""Package the original-style alternating-foot 2x2 walk sheet."""
import argparse
from pathlib import Path
from PIL import Image, ImageOps
from prepare_state_sprites import clean_alpha, keep_largest_component
from generate_lvgl_argb8888 import write_c_image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('c_output', type=Path)
    args = parser.parse_args()
    sheet = Image.open(args.source).convert('RGBA')
    cells, centers = [], []
    for index in range(4):
        col, row = index % 2, index // 2
        cell = sheet.crop((col * sheet.width // 2, row * sheet.height // 2,
                           (col + 1) * sheet.width // 2, (row + 1) * sheet.height // 2))
        cell = keep_largest_component(clean_alpha(cell))
        bounds = cell.getchannel('A').getbbox()
        if bounds is None:
            raise ValueError(f'Empty frame {index}')
        cell = cell.crop(bounds)
        body_x = [x for y in range(cell.height) for x in range(cell.width)
                  if (lambda c: c[3] > 128 and c[0] > 190 and c[1] > 145 and c[2] < 150)(cell.getpixel((x, y)))]
        centers.append((min(body_x) + max(body_x)) / 2)
        cells.append(cell)
    scale = min(164 / max(c.height for c in cells), 158 / max(c.width for c in cells))
    args.output.mkdir(parents=True, exist_ok=True)
    includes = []
    for index, cell in enumerate(cells):
        cell = cell.resize((round(cell.width * scale), round(cell.height * scale)), Image.Resampling.LANCZOS)
        canvas = Image.new('RGBA', (192, 192))
        # Pin the torso top, rather than letting lifted toes shift the body.
        canvas.alpha_composite(cell, (round(96 - centers[index] * scale), 16))
        for direction, frame in [('right', canvas), ('left', ImageOps.mirror(canvas))]:
            name = f'pet_walk_illustrated_{direction}_{index}'
            png = args.output / f'{direction}_{index}.png'
            frame.save(png)
            write_c_image(png, args.c_output.parent / f'{name}.c', name)
            includes.append(f'#include "{name}.c"')
    args.c_output.write_text('\n'.join(includes) + '\n')


if __name__ == '__main__':
    main()
