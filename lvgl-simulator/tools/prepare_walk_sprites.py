#!/usr/bin/env python3
"""Split the generated walk sheet into clean, consistently framed LVGL sprites."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


FRAME_COUNT = 4
CANVAS_SIZE = 192
SOURCE_TOP = 40
SOURCE_HEIGHT = 600
ALPHA_CUTOFF = 128


def clean_alpha(image: Image.Image) -> Image.Image:
    image = image.convert("RGBA")
    alpha = image.getchannel("A").point(lambda value: 255 if value >= ALPHA_CUTOFF else 0)
    image.putalpha(alpha)
    return image


def split(source: Path, output_dir: Path) -> None:
    sheet = clean_alpha(Image.open(source))
    if sheet.width % FRAME_COUNT != 0:
        raise ValueError("Walk sheet width must divide evenly into four frames.")
    output_dir.mkdir(parents=True, exist_ok=True)
    cell_width = sheet.width // FRAME_COUNT
    for index in range(FRAME_COUNT):
        cell = sheet.crop((index * cell_width, SOURCE_TOP,
                           (index + 1) * cell_width, SOURCE_TOP + SOURCE_HEIGHT))
        scale = CANVAS_SIZE / cell_width
        height = round(cell.height * scale)
        frame = cell.resize((CANVAS_SIZE, height), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0))
        canvas.alpha_composite(frame, (0, (CANVAS_SIZE - height) // 2))
        canvas.save(output_dir / f"walk_{index}.png")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    split(args.source, args.output_dir)


if __name__ == "__main__":
    main()
