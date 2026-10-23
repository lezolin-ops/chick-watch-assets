#!/usr/bin/env python3
# 悲伤关键帧整体增益校色：只把身体黄均值拉到 #FDD649，紧 mask 排除腮红/喙/脚/眼/背景。
import os
import numpy as np
from PIL import Image

BASE = '/Users/Zhuanz1/Documents/trae_projects/mini/assets/chick_character_design_pack/sad_keyframes'
OUT = os.path.join(BASE, 'graded')
os.makedirs(OUT, exist_ok=True)
TARGET = np.array([253, 214, 73], dtype=float)
FRAMES = ['frame_01_sad.png', 'frame_02_wronged.png', 'frame_03_peak.png']

def body_mask(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    return (r > 230) & (g > 190) & (b < 115) & ((r - b) > 120)

for f in FRAMES:
    im = Image.open(os.path.join(BASE, f)).convert('RGB')
    a = np.asarray(im).astype(float)
    m = body_mask(a)
    gain = TARGET / a[m].mean(0)
    out = a.copy()
    out[m] = np.clip(a[m] * gain, 0, 255)
    Image.fromarray(out.astype('uint8')).save(os.path.join(OUT, f))
    print(f, 'gain', gain.round(3), 'after', out[m].mean(0).round(1))
