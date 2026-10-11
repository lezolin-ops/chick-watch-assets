#!/usr/bin/env python3
"""Package an ImageGen 4x3 daily-action sheet into grounded LVGL frames."""
import argparse
from pathlib import Path
from PIL import Image, ImageOps
from generate_lvgl_argb8888 import write_c_image
from prepare_state_sprites import clean_alpha, keep_largest_component


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('c_output', type=Path)
    args = parser.parse_args()
    sheet = Image.open(args.source).convert('RGBA')
    cells = []
    for index in range(12):
        col, row = index % 4, index // 4
        cell = sheet.crop((col * sheet.width // 4, row * sheet.height // 3,
                           (col + 1) * sheet.width // 4, (row + 1) * sheet.height // 3))
        cell = keep_largest_component(clean_alpha(cell))
        bounds = cell.getchannel('A').point(lambda a: 255 if a > 32 else 0).getbbox()
        if bounds is None:
            raise ValueError(f'Empty sprite cell {index}')
        cells.append(cell.crop(bounds))
    # One scale for all poses: seated frames must remain shorter than standing.
    scale = min(164 / max(c.width for c in cells), 168 / max(c.height for c in cells))
    args.output.mkdir(parents=True, exist_ok=True)
    includes = []
    for index, cell in enumerate(cells):
        cell = cell.resize((round(cell.width * scale), round(cell.height * scale)), Image.Resampling.LANCZOS)
        canvas = Image.new('RGBA', (192, 192))
        canvas.alpha_composite(cell, ((192 - cell.width) // 2, 180 - cell.height))
        for direction, frame in [('right', canvas), ('left', ImageOps.mirror(canvas))]:
            name = f'pet_daily_{direction}_{index}'
            png = args.output / f'{direction}_{index}.png'
            png_frame = args.c_output.parent / f'{name}.c'
            frame.save(png)
            write_c_image(png, png_frame, name)
            includes.append(f'#include "{name}.c"')
    args.c_output.write_text('\n'.join(includes) + '\n')


if __name__ == '__main__':
    main()
