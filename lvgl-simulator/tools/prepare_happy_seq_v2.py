#!/usr/bin/env python3
"""Extract the confirmed v2 happy mp4 into 192x192 ARGB8888 sequence frames for
the --happy-demo simulator preview. Simulator only; never enters firmware.

Pipeline: ffmpeg 12fps -> key near-white bg + soft shadow to alpha -> 1px edge
min-filter -> character height 156, center x=96, grounded feet y=179, natural
hop offset preserved -> thin to 6fps -> BGRA C descriptor (LVGL ARGB8888).
"""
from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageFilter
import numpy as np

FFMPEG = os.environ.get(
    'FFMPEG',
    '/Users/Zhuanz1/Documents/trae_projects/mini/tools/venv/lib/python3.14/site-packages/imageio_ffmpeg/binaries/ffmpeg-macos-aarch64-v7.1',
)

SRC = Path('/Users/Zhuanz1/Documents/trae_projects/mini/assets/chick_character_design_pack/happy_keyframes/v2_strict/happy_5s.mp4')
OUT = Path('/Users/Zhuanz1/Documents/trae_projects/mini/lvgl-simulator/src/pet_happy_seq.c')

# Benchmark body yellow #FDD649; only the body is graded, blush/beak/feet/eyes untouched.
TARGET_BODY = np.array([253, 214, 73], dtype=float)

CANVAS = 192
CHAR_H = 156
FEET_Y = 179
CENTER_X = 96
SRC_FPS = 12
OUT_FPS = 6
DURATION_MS = 5000


def extract_frames(tmp: Path) -> list[Path]:
    ff = FFMPEG
    subprocess.run([
        ff, '-y', '-i', str(SRC), '-vf', f'fps={SRC_FPS}',
        str(tmp / 'f_%03d.png'),
    ], check=True, capture_output=True)
    return sorted(tmp.glob('f_*.png'))


def key_frame(p: Path) -> Image.Image:
    im = Image.open(p).convert('RGBA')
    px = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            mx, mn = max(r, g, b), min(r, g, b)
            # near-white warm background AND the soft gray ground shadow
            if mn > 205 and (mx - mn) < 30:
                px[x, y] = (r, g, b, 0)
    # eat 1px light fringe
    alpha = im.getchannel('A').filter(ImageFilter.MinFilter(3))
    im.putalpha(alpha)
    return im


def align_body_yellow(im: Image.Image) -> Image.Image:
    """Gentle per-channel gain toward benchmark mean: preserves every pixel's own
    shading (no H/S forcing, no flattening, no highlight wash-out). Tight mask
    keeps blush (b~122) and beak outside the body grade."""
    arr = np.asarray(im).astype(float)
    r, g, b, a = arr[..., 0], arr[..., 1], arr[..., 2], arr[..., 3]
    mask = (r > 200) & (g > 150) & (g < 228) & (b < 115) & ((r - b) > 120) & (a > 0)
    mean = arr[..., :3][mask].mean(0)
    gain = TARGET_BODY / mean
    arr[..., :3][mask] = np.clip(arr[..., :3][mask] * gain, 0, 255)
    return Image.fromarray(arr.astype(np.uint8), 'RGBA')


def bbox(im: Image.Image) -> tuple[int, int, int, int]:
    return im.getchannel('A').getbbox()


def main() -> None:
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        frames = extract_frames(tmp)
        keyed = [align_body_yellow(key_frame(p)) for p in frames]

    # thin to OUT_FPS: pick every (SRC_FPS/OUT_FPS) frame
    step = SRC_FPS // OUT_FPS
    picked = keyed[::step][: DURATION_MS * OUT_FPS // 1000]
    n = len(picked)

    # grounded anchor: frame 0 bottom; preserve relative vertical hop offset
    boxes = [bbox(im) for im in picked]
    scale = CHAR_H / (boxes[0][3] - boxes[0][1])
    ground_bottom_src = boxes[0][3]

    canvases = []
    for im, box in zip(picked, boxes):
        crop = im.crop(box)
        cw, ch = crop.size
        nw = max(1, round(cw * scale))
        nh = max(1, round(ch * scale))
        crop = crop.resize((nw, nh), Image.LANCZOS)
        canvas = Image.new('RGBA', (CANVAS, CANVAS), (0, 0, 0, 0))
        x = CENTER_X - nw // 2
        # natural hop: grounded bottom -> FEET_Y; airborne frames sit higher
        rel_bottom = (box[3] - ground_bottom_src) * scale
        # negative rel_bottom = airborne; feet sit ABOVE the 179 baseline.
        y = FEET_Y + round(rel_bottom) - nh
        if os.environ.get('DEBUG_HOP'):
            print(f'f{len(canvases):02d} src_feet={box[3]} hop_src={ground_bottom_src - box[3]} '
                  f'top={y} bottom={y + nh}')
        canvas.paste(crop, (x, y), crop)
        canvases.append(canvas)

    # QA contact sheet on cream bg
    cell = 192
    sheet = Image.new('RGB', (cell * n, cell), '#FEFCF4')
    for i, cv in enumerate(canvases[:n]):
        sheet.paste(cv, (i * cell, 0), cv)
    qa = OUT.parent / 'happy_seq_v2_qa.png'
    sheet.save(qa)
    print(f'QA sheet: {qa}')

    # emit BGRA C descriptors
    lines = [
        '/* Auto-generated v2 happy sequence (192, simulator preview only, not firmware). */',
        '#include "lvgl/lvgl.h"',
        '',
    ]
    for i, cv in enumerate(canvases[:n]):
        rgba = cv.tobytes()
        bgra = bytearray(len(rgba))
        for o in range(0, len(rgba), 4):
            r, g, b, a = rgba[o:o + 4]
            bgra[o:o + 4] = bytes((b, g, r, a))
        lines.append(f'static const uint8_t ai_happy_{i}_map[] = {{')
        for o in range(0, len(bgra), 16):
            chunk = ', '.join(f'0x{v:02X}' for v in bgra[o:o + 16])
            lines.append(f'    {chunk},')
        lines.append('};')
        lines.append('')
        lines.append(f'const lv_image_dsc_t ai_happy_{i} = {{')
        lines.append('    .header = {')
        lines.append('        .magic = LV_IMAGE_HEADER_MAGIC,')
        lines.append('        .cf = LV_COLOR_FORMAT_ARGB8888,')
        lines.append('        .flags = 0,')
        lines.append(f'        .w = {CANVAS}, .h = {CANVAS}, .stride = {CANVAS*4},')
        lines.append('    },')
        lines.append(f'    .data_size = sizeof(ai_happy_{i}_map),')
        lines.append(f'    .data = ai_happy_{i}_map,')
        lines.append('};')
        lines.append('')

    OUT.write_text('\n'.join(lines))
    print(f'wrote {OUT} frames={n}')


if __name__ == '__main__':
    main()
