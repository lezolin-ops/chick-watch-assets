#!/usr/bin/env python3
"""Export actual ThorVG frames as a five-second looping preview and inspect it."""
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageSequence

ROOT=Path(__file__).resolve().parents[2]
FRAMES=ROOT/'lvgl-simulator/build/idle-trial-preview'
OUT=ROOT/'assets/lottie/idle_trial_v1'

frames=[Image.open(FRAMES/f'frame_{i:03}.ppm').convert('RGB') for i in range(150)]
assert ImageChops.difference(frames[0],frames[-1]).getbbox() is None, 'Loop seam must match'
# Feet are outside the breathing parent and remain pixel-for-pixel stationary.
for frame in frames:
    assert ImageChops.difference(frame.crop((130,422,350,470)),frames[0].crop((130,422,350,470))).getbbox() is None
assert ImageChops.difference(frames[0],frames[33]).getbbox() is not None, 'Breathing must move'
assert ImageChops.difference(frames[45],frames[47]).getbbox() is not None, 'Blink must close'
assert ImageChops.difference(frames[84].crop((40,225,148,350)),frames[99].crop((40,225,148,350))).getbbox() is not None
durations=[30,30,40]*50  # GIF timing resolution is 10ms; total is exactly 5000ms.
frames[0].save(OUT/'preview.gif',save_all=True,append_images=frames[1:],duration=durations,loop=0)
frames[0].save(OUT/'poster.png')
gif=Image.open(OUT/'preview.gif')
assert sum(f.info.get('duration',0) for f in ImageSequence.Iterator(gif))==5000
sheet=Image.new('RGB',(960,520),'white'); draw=ImageDraw.Draw(sheet)
for j,index in enumerate([0,33,45,47,49,84,99,149]):
    x,y=(j%4)*240,(j//4)*260
    sheet.paste(frames[index].resize((240,240)),(x,y+20))
    draw.text((x+8,y+5),f'{index/30:.2f}s / frame {index}',fill='black')
sheet.save(OUT/'motion_review.png')
for i in [0,33,45,47,49,84,99,149]: frames[i].save(OUT/f'keyframe_{i:03}.png')
print('PASS: 5.000s GIF, exact loop seam, pixel-stable feet, breathing/blink/wing motion.')
print(OUT/'preview.gif')
