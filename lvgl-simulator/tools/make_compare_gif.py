#!/usr/bin/env python3
"""Side-by-side GIF of the three happy-animation variants.

Left  : AI 30-frame baked sequence   (--happy-demo, 5000ms)
Mid   : firmware default bounce      (--happy-once, 1600ms, one base sprite)
Right : hybrid 6 AI keyframes + code (--happy-keys, 1600ms)
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

FRAMES = Path('E:/Projects/tools')
OUT = Path('E:/Projects/chick-watch-assets/lvgl-simulator/tools/happy_compare.gif')
N = 60
MS = 66
CROP = (105, 200, 305, 410)  # character box in the 410x502 screen
GAP = 6


def load(d: str):
    return [Image.open(FRAMES / f'frames_{d}' / f'f_{i:03d}.png').convert('RGB').crop(CROP)
            for i in range(N)]


sets = {
    'AI 30帧 baked': load('ai_seq'),
    '真机默认 弹跳': load('firmware'),
    '混合 6帧+插值': load('kf_crop'),
}
labels = list(sets)
w, h = sets[labels[0]][0].size
canvas = (w * 3 + GAP * 4, h + 26)

out = []
for i in range(N):
    frame = Image.new('RGB', canvas, (255, 255, 255))
    d = ImageDraw.Draw(frame)
    for j, name in enumerate(labels):
        x = GAP + j * (w + GAP)
        frame.paste(sets[name][i], (x, 26))
        d.text((x + 4, 6), name, fill=(60, 60, 60))
    d.text((GAP, h + 12), f'{i * MS} ms', fill=(120, 120, 120))
    out.append(frame)

out[0].save(OUT, save_all=True, append_images=out[1:], duration=MS, loop=0)
print(f'wrote {OUT}  ({OUT.stat().st_size / 1024:.0f} KB)  {len(out)} frames @ {MS}ms')
