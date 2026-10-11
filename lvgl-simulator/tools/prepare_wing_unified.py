"""Align the consistent round wing drawings into shared side/front rig frames."""
from pathlib import Path
import sys
from math import atan2,degrees
from PIL import Image,ImageOps
from prepare_state_sprites import clean_alpha,keep_largest_component
from generate_lvgl_argb8888 import write_c_image
ROOT=Path(__file__).resolve().parents[2]
sheet=Image.open(Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'assets/chick_character_design_pack/wing_unified_source_v2.png').convert('RGBA')
angles=[0,5,12,20,12,5]
for view,row,target_width in [('side',0,42),('front',2,28)]:
    cells=[]
    # Delivered sheet has two copies per view; consume the first of each pair.
    for i in range(6):
        cell=keep_largest_component(clean_alpha(sheet.crop((i*sheet.width//6,row*sheet.height//4,(i+1)*sheet.width//6,(row+1)*sheet.height//4))))
        cell=cell.crop(cell.getchannel('A').getbbox()); cells.append(cell)
    scale=target_width/max(c.width for c in cells)
    folder=ROOT/'assets/runtime/chick'/('wing_frames' if view=='side' else 'front_wing_frames')
    folder.mkdir(parents=True,exist_ok=True)
    includes=[]; base_angle=0
    for i,cell in enumerate(cells):
        cell=cell.resize((round(cell.width*scale),round(cell.height*scale)),Image.Resampling.LANCZOS)
        alpha=cell.getchannel('A')
        points=[(x,y) for x in range(cell.width-3,cell.width) for y in range(cell.height) if alpha.getpixel((x,y))>160]
        rx=round(sum(x for x,y in points)/len(points)); ry=round(sum(y for x,y in points)/len(points))
        canvas=Image.new('RGBA',(64,64)); canvas.alpha_composite(cell,(54-rx,34-ry))
        mass=[(x,y) for y in range(64) for x in range(64) if canvas.getpixel((x,y))[3]>160]
        cx=sum(x for x,y in mass)/len(mass); cy=sum(y for x,y in mass)/len(mass)
        current=degrees(atan2(cy-34,cx-54))
        if i==0: base_angle=current
        correction=(base_angle+angles[i]-current+180)%360-180
        canvas=canvas.rotate(-correction,resample=Image.Resampling.BICUBIC,center=(54,34))
        for direction,frame in [('right',canvas),('left',ImageOps.mirror(canvas))]:
            name=f'pet_wing_frame_{i}_{direction}' if view=='side' else f'pet_front_wing_frame_{i}_{direction}'
            png=folder/f'{i}_{direction}.png'; frame.save(png)
            write_c_image(png,ROOT/'lvgl-simulator/src'/f'{name}.c',name)
            includes.append(f'#include "{name}.c"')
        print(view,i,'mass',len(mass),'align',round(correction,1))
    aggregate='pet_wing_frames.c' if view=='side' else 'pet_front_wing_frames.c'
    (ROOT/'lvgl-simulator/src'/aggregate).write_text('\n'.join(includes)+'\n')
