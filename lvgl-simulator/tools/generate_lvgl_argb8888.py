#!/usr/bin/env python3
"""Convert a transparent PNG into an LVGL ARGB8888 C image descriptor.

LVGL ARGB8888 pixels are stored as BGRA bytes on little-endian targets.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


def write_c_image(source: Path, destination: Path, name: str) -> None:
    image = Image.open(source).convert("RGBA")
    width, height = image.size
    rgba = image.tobytes()
    bgra = bytearray(len(rgba))

    for offset in range(0, len(rgba), 4):
        red, green, blue, alpha = rgba[offset : offset + 4]
        bgra[offset : offset + 4] = bytes((blue, green, red, alpha))

    lines = [
        "/* Auto-generated from a transparent PNG. Do not edit by hand. */",
        "#include \"lvgl/lvgl.h\"",
        "",
        f"static const uint8_t {name}_map[] = {{",
    ]
    for offset in range(0, len(bgra), 16):
        chunk = ", ".join(f"0x{value:02X}" for value in bgra[offset : offset + 16])
        lines.append(f"    {chunk},")
    lines.extend(
        [
            "};",
            "",
            f"const lv_image_dsc_t {name} = {{",
            "    .header = {",
            "        .magic = LV_IMAGE_HEADER_MAGIC,",
            "        .cf = LV_COLOR_FORMAT_ARGB8888,",
            "        .flags = 0,",
            f"        .w = {width},",
            f"        .h = {height},",
            f"        .stride = {width * 4},",
            "    },",
            f"    .data_size = sizeof({name}_map),",
            f"    .data = {name}_map,",
            "};",
            "",
        ]
    )
    destination.write_text("\n".join(lines))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--name", required=True)
    args = parser.parse_args()
    write_c_image(args.source, args.destination, args.name)


if __name__ == "__main__":
    main()
