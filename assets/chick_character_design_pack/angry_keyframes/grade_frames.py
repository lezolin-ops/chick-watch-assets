#!/usr/bin/env python3
# 生气关键帧整体增益校色：只把身体黄均值拉到 #FDD649，逐通道乘增益，保留明暗渐变。
# 紧 mask 排除腮红/喙/脚/眼/背景，禁止逐像素 H/S。
import os
import numpy as np
from PIL import Image

BASE = '/Users/Zhuanz1/Documents/trae_projects/mini/assets/chick_character_design_pack/angry_keyframes'
OUT = os.path.join(BASE, 'graded')
os.makedirs(OUT, exist_ok=True)
TARGET = np.array([253, 214, 73], dtype=float)  # #FDD649
FRAMES = ['frame_00_neutral.png',
          'frame_01_pout.png', 'frame_02_shout.png',
          'frame_03_rage.png', 'frame_04_reset.png',
          'frame_05_raise.png', 'frame_06_stomp.png']

def body_mask(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    # 身体黄：高 r、高 g、中低 b；g>190 排除腮红(g~168)/喙脚(g<170)，b<115 排除腮红(b~121)
    return (r > 230) & (g > 190) & (b < 115) & ((r - b) > 120)

for f in FRAMES:
    im = Image.open(os.path.join(BASE, f)).convert('RGB')
    a = np.asarray(im).astype(float)
    m = body_mask(a)
    mean = a[m].mean(0)
    gain = TARGET / mean
    out = a.copy()
    out[m] = np.clip(a[m] * gain, 0, 255)
    res = Image.fromarray(out.astype(np.uint8))
    res.save(os.path.join(OUT, f))
    new_mean = out[m].mean(0)
    print(f, 'before', mean.round(1), 'gain', gain.round(3), 'after', new_mean.round(1))
