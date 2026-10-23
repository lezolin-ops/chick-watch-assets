#!/usr/bin/env python3
# 生气循环预览：6 张校色关键帧，按黄色身体等比缩放整帧（保留脚/翅），身体尺寸跨帧一致，硬切 hold。
# 跺脚=S1提脚→S2踩下；GIF 仅节奏预览，不作素材。
import os
import numpy as np
from PIL import Image

BASE = '/Users/Zhuanz1/Documents/trae_projects/mini/assets/chick_character_design_pack/angry_keyframes'
G = os.path.join(BASE, 'graded')
SEQ = [
    ('frame_01_pout.png', 8),
    ('frame_02_shout.png', 8),
    ('frame_03_rage.png', 6),
    ('frame_05_raise.png', 3),
    ('frame_06_stomp.png', 5),
    ('frame_04_reset.png', 8),
]
SIZE = 480
FPS = 12
BG = (254, 252, 244)
TARGET_BODY_H = int(SIZE * 0.62)
BODY_TOP = int(SIZE * 0.16)

def render(path):
    im = Image.open(path).convert('RGB')
    a = np.asarray(im)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    yellow = (r > 230) & (g > 190) & (b < 115) & ((r - b) > 120)
    ys, xs = np.where(yellow)
    y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
    s = TARGET_BODY_H / (y1 - y0)
    im2 = im.resize((round(im.width * s), round(im.height * s)))
    # 缩放后身体 bbox 位置
    bx0, by0 = x0 * s, y0 * s
    cv = Image.new('RGB', (SIZE, SIZE), BG)
    # 身体水平居中、顶部对齐 BODY_TOP
    px = (SIZE - (x1 - x0) * s) / 2 - bx0
    py = BODY_TOP - by0
    cv.paste(im2, (round(px), round(py)))
    return cv

seq = []
for f, n in SEQ:
    seq += [render(os.path.join(G, f))] * n

out = os.path.join(BASE, 'angry_loop_preview.gif')
seq[0].save(out, save_all=True, append_images=seq[1:], loop=0,
            duration=int(1000 / FPS), optimize=False, disposal=2)
print('frames', len(seq), 'duration_s', round(len(seq) / FPS, 2), out)
