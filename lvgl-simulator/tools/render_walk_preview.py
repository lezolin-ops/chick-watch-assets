#!/usr/bin/env python3
"""Export offscreen LVGL renderings as an animation and a pose contact sheet."""
from pathlib import Path
from PIL import Image, ImageDraw

folder = Path(__file__).resolve().parents[1] / 'build/animation-preview'
tuft_frames=[Image.open(folder/f'tuft-clip-{i:02}.ppm').convert('RGB') for i in range(100)]
tuft_frames[0].save(folder/'tuft-follow-preview.gif',save_all=True,append_images=tuft_frames[1:],duration=80,loop=0)
for view in range(2):
    front_frames=[Image.open(folder/f'front-flap-{view}-{i:02}.ppm').convert('RGB') for i in range(70)]
    front_frames[0].save(folder/f'front-flap-{view}-preview.gif',save_all=True,append_images=front_frames[1:],duration=40,loop=0)
front_sheet=Image.new('RGB',(1020,470),'white'); front_draw=ImageDraw.Draw(front_sheet)
for view in range(2):
    for j,i in enumerate([0,18,24,30,39,69]):
        detail=Image.open(folder/f'front-flap-{view}-{i:02}.ppm').crop((120,210,290,410))
        front_sheet.paste(detail,(j*170,view*235+25))
        front_draw.text((j*170+6,view*235+6),f'{"Stand" if view else "Sit"} / {(i+1)*40}ms',fill='black')
front_sheet.save(folder/'front-flap-review.png')
wing_frames=[Image.open(folder/f'wing-flap-{i:02}.ppm').convert('RGB') for i in range(30)]
wing_frames[0].save(folder/'wing-flap-preview.gif',save_all=True,append_images=wing_frames[1:],duration=40,loop=0)
wing_sheet=Image.new('RGB',(1020,470),'white'); wing_draw=ImageDraw.Draw(wing_sheet)
for direction in range(2):
    for frame in range(6):
        detail=Image.open(folder/f'wing-pose-{direction}-{frame}.ppm').crop((120,210,290,410))
        wing_sheet.paste(detail,(frame*170,direction*235+25))
        wing_draw.text((frame*170+6,direction*235+6),f'{"Left" if direction else "Right"} / {frame}',fill='black')
wing_sheet.save(folder/'wing-frames-review.png')
ground_frames=[Image.open(folder/f'ground-clip-{i:03}.ppm').convert('RGB') for i in range(110)]
ground_frames[0].save(folder/'ground-look-preview.gif',save_all=True,append_images=ground_frames[1:],duration=40,loop=0)
sky_frames=[Image.open(folder/f'sky-clip-{i:03}.ppm').convert('RGB') for i in range(110)]
sky_frames[0].save(folder/'sky-look-preview.gif',save_all=True,append_images=sky_frames[1:],duration=40,loop=0)
quiet_frames=[Image.open(folder/f'quiet-branch-{i:02}.ppm').convert('RGB') for i in range(75)]
quiet_frames[0].save(folder/'quiet-observe-preview.gif',save_all=True,append_images=quiet_frames[1:],duration=80,loop=0)
idle_frames=[Image.open(folder/f'idle-clip-{i:02}.ppm').convert('RGB') for i in range(100)]
idle_frames[0].save(folder/'idle-refined-preview.gif',save_all=True,append_images=idle_frames[1:],duration=40,loop=0)
head_sheet=Image.new('RGB',(880,560),'white')
head_draw=ImageDraw.Draw(head_sheet)
for row,direction in enumerate(['left','right']):
    for i in range(4):
        frame=Image.open(folder/f'head-{direction}-{i}.ppm')
        detail=frame.crop((120,210,290,410)).resize((220,259),Image.Resampling.NEAREST)
        head_sheet.paste(detail,(i*220,row*280+21))
        head_draw.text((i*220+6,row*280+5),f'{direction} / {i*15} deg',fill='black')
head_sheet.save(folder/'head-turn-review.png')
clip=[]
for i,count in [(0,12),(1,3),(2,3),(3,30),(2,3),(1,3),(0,12)]:
    clip.extend([Image.open(folder/f'head-right-{i}.ppm').convert('RGB')]*count)
for i,count in [(1,3),(2,3),(3,30),(2,3),(1,3),(0,12)]:
    clip.extend([Image.open(folder/f'head-left-{i}.ppm').convert('RGB')]*count)
clip[0].save(folder/'head-turn-preview.gif',save_all=True,append_images=clip[1:],duration=40,loop=0)
# Full-body inspection must preserve aspect ratio, not stretch the chick.
body_sheet=Image.new('RGB',(880,840),'white')
body_draw=ImageDraw.Draw(body_sheet)
for i in range(12):
    detail=Image.open(folder/f'gait-{i:02}.ppm').crop((120,210,290,410))
    detail=detail.resize((220,259),Image.Resampling.NEAREST)
    x,y=i%4*220,i//4*280
    body_sheet.paste(detail,(x,y+21))
    body_draw.text((x+8,y+5),f'Step {i/12:.2f}',fill='black')
body_sheet.save(folder/'expressive-walk-review.png')
sheet = Image.new('RGB', (1280, 564), '#ffffff')
draw = ImageDraw.Draw(sheet)
for index in range(12):
    frame = Image.open(folder / f'gait-{index:02}.ppm')
    detail = frame.crop((125, 325, 285, 405)).resize((320, 160), Image.Resampling.NEAREST)
    x, y = index % 4 * 320, index // 4 * 188
    sheet.paste(detail, (x, y + 28))
    draw.text((x + 8, y + 7), f'Cycle {index / 12:.2f}', fill='#202020')
sheet.save(folder / 'gait-contact-sheet.png')
frames = [Image.open(folder / f'walk-clip-{index:02}.ppm').convert('RGB') for index in range(90)]
frames[0].save(folder / 'walk-preview.gif', save_all=True, append_images=frames[1:], duration=80, loop=0)
for name in ['walk-right', 'walk-left-closed', 'rest-open', 'rest-closed']:
    Image.open(folder / f'{name}.ppm').save(folder / f'{name}.png')
print(folder / 'gait-contact-sheet.png')
print(folder / 'walk-preview.gif')
mouth_sheet = Image.new('RGB', (720, 450), '#ffffff')
mouth_draw = ImageDraw.Draw(mouth_sheet)
for view in range(3):
    for stage in range(3):
        frame = Image.open(folder / f'mouth-{view}-{stage}.ppm')
        # Face details from front, quarter and side views.
        detail = frame.crop((165, 263, 290, 320)).resize((240, 120), Image.Resampling.NEAREST)
        mouth_sheet.paste(detail, (stage * 240, view * 150 + 30))
        mouth_draw.text((stage*240+8, view*150+8), ['Closed', 'Slightly open', 'Open'][stage], fill='#202020')
mouth_sheet.save(folder / 'mouth-contact-sheet.png')
mouth_frames = [Image.open(folder / f'mouth-clip-{i:02}.ppm').convert('RGB') for i in range(30)]
mouth_frames[0].save(folder / 'mouth-preview.gif', save_all=True, append_images=mouth_frames[1:], duration=40, loop=0)
print(folder / 'mouth-contact-sheet.png')
print(folder / 'mouth-preview.gif')
side_frames=[Image.open(folder/f'side-mouth-clip-{i:02}.ppm').convert('RGB') for i in range(30)]
side_frames[0].save(folder/'side-mouth-preview.gif',save_all=True,append_images=side_frames[1:],duration=40,loop=0)
side_sheet=Image.new('RGB',(990,390),'#ffffff')
labels=ImageDraw.Draw(side_sheet)
for stage in range(3):
    frame=Image.open(folder/f'mouth-2-{stage}.ppm')
    detail=frame.crop((125,215,290,395)).resize((330,360),Image.Resampling.NEAREST)
    side_sheet.paste(detail,(stage*330,30))
    labels.text((stage*330+10,9),['Closed','Slightly open','Open'][stage],fill='#202020')
side_sheet.save(folder/'side-mouth-review.png')
print(folder/'side-mouth-review.png')
print(folder/'side-mouth-preview.gif')
atmosphere=[Image.open(folder/f'atmosphere-{i:02}.ppm').convert('RGB') for i in range(70)]
atmosphere[0].save(folder/'atmosphere-preview.gif',save_all=True,append_images=atmosphere[1:],duration=100,loop=0)
palette=Image.new('RGB',(820,532),'#ffffff'); caption=ImageDraw.Draw(palette)
for i,name in enumerate(['front','side']):
    palette.paste(Image.open(folder/f'palette-{name}.ppm'),(410*i,30))
    caption.text((410*i+10,9),['Front: same intrinsic palette','Side: same intrinsic palette'][i],fill='#202020')
palette.save(folder/'palette-scene-review.png')
atmosphere[0].save(folder/'scene-review.png')
print(folder/'atmosphere-preview.gif')
