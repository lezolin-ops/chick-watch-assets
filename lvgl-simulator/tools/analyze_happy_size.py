#!/usr/bin/env python3
"""Measure how much the 6 AI happy keyframes can be shrunk.

Reads the baked ARGB8888 descriptors straight out of src/pet_happy_frames.c,
then reports, for every candidate encoding, the real byte cost that would land
in firmware .rodata (not the size of the C source text).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

import numpy as np

SRC = Path(__file__).resolve().parents[1] / 'src' / 'pet_happy_frames.c'
W = H = 192
NFRAMES = 6

text = SRC.read_text(encoding='utf-8', errors='ignore')


def load(idx: int) -> np.ndarray:
    m = re.search(rf'pet_happy_{idx}_map\[\] = \{{(.*?)\}};', text, re.S)
    if not m:
        sys.exit(f'pet_happy_{idx}_map not found')
    body = m.group(1)
    vals = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', body)]
    arr = np.frombuffer(bytes(vals), dtype=np.uint8)
    expect = W * H * 4
    if arr.size != expect:
        sys.exit(f'frame {idx}: got {arr.size} bytes, expected {expect}')
    # LVGL ARGB8888 in memory is B,G,R,A (little-endian 0xAARRGGBB)
    return arr.reshape(H, W, 4)


frames = [load(i) for i in range(NFRAMES)]
alpha = np.stack([f[:, :, 3] for f in frames])

# union bounding box across all frames (shared canvas keeps poses aligned)
ys, xs = np.where(alpha.max(axis=0) > 8)
x0, x1 = int(xs.min()), int(xs.max())
y0, y1 = int(ys.min()), int(ys.max())
cw, ch = x1 - x0 + 1, y1 - y0 + 1

print(f'canvas            : {W}x{H}  ({W*H*4/1024:.1f} KB/frame ARGB8888)')
print(f'union bbox        : x {x0}..{x1}, y {y0}..{y1}  ->  {cw}x{ch}')
print(f'bbox area share   : {cw*ch/(W*H)*100:.1f}% of the 192x192 canvas')
for i in range(NFRAMES):
    cov = int((alpha[i] > 8).sum())
    print(f'  frame {i}: opaque px {cov:6d} ({cov/(W*H)*100:5.1f}%)')

full = W * H * 4 * NFRAMES
cropped = cw * ch * 4 * NFRAMES
print()
print(f'1) raw ARGB8888          : {full/1024:8.1f} KB   (what ships today)')
print(f'2) cropped ARGB8888      : {cropped/1024:8.1f} KB   (-{(1-cropped/full)*100:.1f}%, lossless)')
print(f'3) cropped ARGB8565/RGB565A8 (3B/px): {cw*ch*3*NFRAMES/1024:8.1f} KB   '
      f'(-{(1-cw*ch*3*NFRAMES/full)*100:.1f}%)')

# palette feasibility: unique colours over the cropped, non-transparent pixels
sub = np.stack([f[y0:y1 + 1, x0:x1 + 1] for f in frames])
opaque = sub[..., 3] > 8
rgba = sub.reshape(-1, 4)[opaque.reshape(-1)]
uniq = np.unique(rgba, axis=0)
print(f'\nunique RGBA colours (cropped, alpha>8): {uniq.shape[0]}')
pal = cw * ch * 1 * NFRAMES + uniq.shape[0] * 4
print(f'4) cropped L8 palette     : {pal/1024:8.1f} KB   '
      f'({uniq.shape[0]} colours, needs a custom decoder for per-pixel alpha)'
      if uniq.shape[0] <= 256 else
      f'4) L8 palette NOT viable  : {uniq.shape[0]} unique colours > 256')

# run-length over cropped ARGB8888 rows (LVGL-style RLE on 4-byte pixels)
def rle_cost(f: np.ndarray) -> int:
    px = f.reshape(-1, 4)
    # change mask vs previous pixel
    chg = np.any(px[1:] != px[:-1], axis=1)
    runs = 1 + int(chg.sum())
    return runs * (4 + 1)  # ARGB8888 RLE: 1 byte header + pixel


rle = sum(rle_cost(f) for f in sub)
print(f'5) cropped ARGB8888 + RLE : {rle/1024:8.1f} KB   '
      f'(-{(1-rle/full)*100:.1f}%)')

# how flat is the art? share of pixels covered by the top-N colours
flat = np.stack([f[y0:y1 + 1, x0:x1 + 1] for f in frames])
px = flat.reshape(-1, 4)
mask = px[:, 3] > 8
vals, counts = np.unique(px[mask], axis=0, return_counts=True)
order = np.argsort(-counts)
cum = np.cumsum(counts[order]) / counts.sum()
for n in (16, 32, 64, 128, 256):
    print(f'   top {n:3d} colours cover {cum[n-1]*100:5.1f}% of opaque pixels')
