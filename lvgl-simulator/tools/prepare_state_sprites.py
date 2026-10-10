#!/usr/bin/env python3
"""Extract clean state sprites from the approved character-state sheet."""

from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path

from PIL import Image


STATE_CELLS = {"hungry": (1, 1), "sleep": (1, 2)}
ALPHA_CUTOFF = 128
CANVAS_SIZE = 192


def clean_alpha(image: Image.Image) -> Image.Image:
    image = image.convert("RGBA")
    image.putalpha(image.getchannel("A").point(lambda value: 255 if value >= ALPHA_CUTOFF else 0))
    return image


def keep_largest_component(image: Image.Image) -> Image.Image:
    """Drop neighboring poses that overlap a state-sheet cell boundary."""
    alpha = image.getchannel("A")
    width, height = image.size
    pixels = bytearray(alpha.tobytes())
    visited = bytearray(len(pixels))
    largest: list[int] = []

    for start, value in enumerate(pixels):
        if value == 0 or visited[start]:
            continue
        component: list[int] = []
        queue = deque([start])
        visited[start] = 1
        while queue:
            index = queue.popleft()
            component.append(index)
            x, y = index % width, index // width
            for neighbor in (index - 1 if x else -1,
                             index + 1 if x + 1 < width else -1,
                             index - width if y else -1,
                             index + width if y + 1 < height else -1):
                if neighbor >= 0 and not visited[neighbor] and pixels[neighbor]:
                    visited[neighbor] = 1
                    queue.append(neighbor)
        if len(component) > len(largest):
            largest = component

    clean = bytearray(len(pixels))
    for index in largest:
        clean[index] = 255
    image.putalpha(Image.frombytes("L", image.size, bytes(clean)))
    return image


def extract(source: Path, output_dir: Path) -> None:
    sheet = clean_alpha(Image.open(source))
    cell_w, cell_h = sheet.width // 3, sheet.height // 2
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, (row, column) in STATE_CELLS.items():
        cell = sheet.crop((column * cell_w, row * cell_h,
                           (column + 1) * cell_w, (row + 1) * cell_h))
        cell = keep_largest_component(cell)
        bounds = cell.getchannel("A").getbbox()
        if bounds is None:
            raise ValueError(f"{name} cell is empty")
        subject = cell.crop(bounds)
        scale = min(168 / subject.height, 164 / subject.width)
        subject = subject.resize((round(subject.width * scale), round(subject.height * scale)), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0))
        canvas.alpha_composite(subject, ((CANVAS_SIZE - subject.width) // 2, 180 - subject.height))
        canvas.save(output_dir / f"state_{name}.png")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    extract(args.source, args.output_dir)


if __name__ == "__main__":
    main()
