#!/usr/bin/env python3
"""Build the angry loop keyframes into 192x192 ARGB8888 frames for the
--angry-demo simulator workbench. Simulator only; never enters firmware.

All frames are already body-graded PNGs. Key out the warm off-white background,
apply ONE global scale from A1 (no per-frame rescale -> no size pop), anchor by
yellow-body center so the raised foot floats instead of dropping.
"""
from pathlib import Path
from PIL import Image, ImageFilter
import numpy as np

ROOT = Path('/Users/Zhuanz1/Documents/trae_projects/mini/assets/chick_character_design_pack/angry_keyframes/graded')
OUT = Path('/Users/Zhuanz1/Documents/trae_projects/mini/lvgl-simulator/src/pet_angry_seq.c')

FRAMES = [
    ('frame_01_pout.png', 'A1'),
    ('frame_02_shout.png', 'A2'),
    ('frame_03_rage.png', 'A3'),
    ('frame_05_raise.png', 'S1'),
    ('frame_06_stomp.png', 'S2'),
    ('frame_04_reset.png', 'A4'),
]

CANVAS = 192
CHAR_H = 156
CENTER_X = 96
FEET_Y = 179


def key_bg(im: Image.Image) -> Image.Image:
    im = im.convert('RGBA')
    arr = np.asarray(im).copy()
    r, g, b, a = arr[..., 0], arr[..., 1], arr[..., 2], arr[..., 3]
    # warm off-white background #FEFCF4 and near-white fringe
    bg = (r > 232) & (g > 226) & (b > 205) & ((r - b) < 40)
    arr[bg, 3] = 0
    out = Image.fromarray(arr, 'RGBA')
    out.putalpha(out.getchannel('A').filter(ImageFilter.MinFilter(3)))
    return out


def yellow_box(im: Image.Image):
    a = np.asarray(im)
    r, g, b, al = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    m = (r > 200) & (g > 150) & (b < 115) & ((r - b) > 120) & (al > 0)
    ys, xs = np.where(m)
    return xs.min(), ys.min(), xs.max(), ys.max()


def main():
    keyed = [key_bg(Image.open(ROOT / f)) for f, _ in FRAMES]
    # global scale from A1 whole character bbox
    b0 = keyed[0].getchannel('A').getbbox()
    scale = CHAR_H / (b0[3] - b0[1])
    # A1 yellow-body center (anchor)
    yx0, yy0, yx1, yy1 = yellow_box(keyed[0])
    a1 = keyed[0]
    ab = a1.getchannel('A').getbbox()
    a1_crop = a1.crop(ab)
    nw = round(a1_crop.width * scale)
    nh = round(a1_crop.height * scale)
    a1_s = a1_crop.resize((nw, nh), Image.LANCZOS)
    a1_top = FEET_Y - nh
    a1_body_cy = a1_top + ((yy0 + yy1) / 2 - ab[1]) * scale

    canvases = []
    qa = []
    for im, (fn, lab) in zip(keyed, FRAMES):
        ab = im.getchannel('A').getbbox()
        crop = im.crop(ab)
        cw, ch = crop.size
        nw, nh = round(cw * scale), round(ch * scale)
        crop = crop.resize((nw, nh), Image.LANCZOS)
        canvas = Image.new('RGBA', (CANVAS, CANVAS), (0, 0, 0, 0))
        x = CENTER_X - nw // 2
        # anchor on yellow-body center y; feet baseline only for standing default
        ybx0, yby0, ybx1, yby1 = yellow_box(im)
        body_cy = ((yby0 + yby1) / 2 - ab[1]) * scale
        y = round(a1_body_cy - body_cy)
        canvas.paste(crop, (x, y), crop)
        canvases.append(canvas)
        qa.append((lab, canvas))

    # QA sheet on cream
    sheet = Image.new('RGB', (CANVAS * len(canvases), CANVAS + 24), '#FEFCF4')
    from PIL import ImageDraw, ImageFont
    d = ImageDraw.Draw(sheet)
    fp = '/System/Library/Fonts/Hiragino Sans GB.ttc'
    font = ImageFont.truetype(fp, 14) if Path(fp).exists() else ImageFont.load_default()
    for i, (lab, cv) in enumerate(qa):
        sheet.paste(cv, (i * CANVAS, 0), cv)
        d.text((i * CANVAS + 8, CANVAS + 4), lab, fill='#333', font=font)
    sheet.save(OUT.parent / 'angry_seq_qa.png')

    lines = [
        '/* Auto-generated angry keyframe loop (192, simulator preview only, not firmware). */',
        '#include "lvgl/lvgl.h"',
        '',
    ]
    for i, cv in enumerate(canvases):
        rgba = cv.tobytes()
        bgra = bytearray(len(rgba))
        for o in range(0, len(rgba), 4):
            r, g, b, a = rgba[o:o+4]
            bgra[o:o+4] = bytes((b, g, r, a))
        lines.append(f'static const uint8_t ai_angry_{i}_map[] = {{')
        for o in range(0, len(bgra), 16):
            lines.append('    ' + ', '.join(f'0x{v:02X}' for v in bgra[o:o+16]) + ',')
        lines.append('};')
        lines.append('')
        lines.append(f'const lv_image_dsc_t ai_angry_{i} = {{')
        lines.append('    .header = {')
        lines.append('        .magic = LV_IMAGE_HEADER_MAGIC,')
        lines.append('        .cf = LV_COLOR_FORMAT_ARGB8888,')
        lines.append('        .flags = 0,')
        lines.append(f'        .w = {CANVAS}, .h = {CANVAS}, .stride = {CANVAS*4},')
        lines.append('    },')
        lines.append(f'    .data_size = sizeof(ai_angry_{i}_map),')
        lines.append(f'    .data = ai_angry_{i}_map,')
        lines.append('};')
        lines.append('')
    OUT.write_text('\n'.join(lines))
    print(f'wrote {OUT} frames={len(canvases)}')


if __name__ == '__main__':
    main()
