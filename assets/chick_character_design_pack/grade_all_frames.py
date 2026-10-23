#!/usr/bin/env python3
# 通用整体增益校色：把各关键帧目录根下的 frame_*.png 校色到对应 graded/ 目录。
# 只动身体黄（紧 mask），逐通道乘 帧均值→#FDD649 的增益，保留明暗渐变；不强制 H/S。
import os, glob
import numpy as np
from PIL import Image

PACK = '/Users/Zhuanz1/Documents/trae_projects/mini/assets/chick_character_design_pack'
TARGET = np.array([253, 214, 73], dtype=float)

def body_mask(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    return (r > 230) & (g > 190) & (b < 115) & ((r - b) > 120)

for f in sorted(glob.glob(os.path.join(PACK, '*_keyframes', 'frame_*.png'))):
    d = os.path.dirname(f)
    outdir = os.path.join(d, 'graded')
    os.makedirs(outdir, exist_ok=True)
    im = Image.open(f).convert('RGB')
    a = np.asarray(im).astype(float)
    m = body_mask(a)
    if m.sum() < 500:
        print('SKIP (no body mask)', f); continue
    gain = TARGET / a[m].mean(0)
    out = a.copy(); out[m] = np.clip(a[m] * gain, 0, 255)
    Image.fromarray(out.astype('uint8')).save(os.path.join(outdir, os.path.basename(f)))
    print(os.path.relpath(f, PACK), 'gain', gain.round(3))
