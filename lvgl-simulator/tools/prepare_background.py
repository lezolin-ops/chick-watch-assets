#!/usr/bin/env python3
"""Resize background artwork to the Waveshare watch's 410x502 canvas."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


DISPLAY_SIZE = (410, 502)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()

    image = Image.open(args.source).convert("RGB")
    target_ratio = DISPLAY_SIZE[0] / DISPLAY_SIZE[1]
    source_ratio = image.width / image.height
    if source_ratio > target_ratio:
        crop_width = round(image.height * target_ratio)
        left = (image.width - crop_width) // 2
        image = image.crop((left, 0, left + crop_width, image.height))
    elif source_ratio < target_ratio:
        crop_height = round(image.width / target_ratio)
        top = (image.height - crop_height) // 2
        image = image.crop((0, top, image.width, top + crop_height))
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    image.resize(DISPLAY_SIZE, Image.Resampling.LANCZOS).save(args.destination, optimize=True)


if __name__ == "__main__":
    main()
