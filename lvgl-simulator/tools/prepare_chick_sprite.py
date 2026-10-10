#!/usr/bin/env python3
"""Create a clean 192 px runtime sprite from the character master artwork."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


CANVAS_SIZE = 192
TOP_MARGIN = 12
BOTTOM_MARGIN = 12
ALPHA_CUTOFF = 128


def clean_alpha(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A").point(lambda value: 255 if value >= ALPHA_CUTOFF else 0)
    rgba.putalpha(alpha)
    return rgba


def prepare(source: Path, destination: Path) -> None:
    master = clean_alpha(Image.open(source))
    bounds = master.getchannel("A").getbbox()
    if bounds is None:
        raise ValueError("The source contains no visible pixels.")

    subject = master.crop(bounds)
    max_height = CANVAS_SIZE - TOP_MARGIN - BOTTOM_MARGIN
    scale = min(max_height / subject.height, (CANVAS_SIZE - 32) / subject.width)
    size = (round(subject.width * scale), round(subject.height * scale))
    subject = subject.resize(size, Image.Resampling.LANCZOS)

    sprite = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0))
    offset = ((CANVAS_SIZE - subject.width) // 2, TOP_MARGIN)
    sprite.alpha_composite(subject, offset)
    sprite.save(destination)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    prepare(args.source, args.destination)


if __name__ == "__main__":
    main()
