"""Pack original-sized frontal wing regions; original face/feet stay in runtime."""
from pathlib import Path
import sys
import re
from PIL import Image
from prepare_state_sprites import clean_alpha,keep_largest_component
from prepare_blink_geometry import geometry
from generate_lvgl_argb8888 import write_c_image
ROOT=Path(__file__).resolve().parents[2]
sheet=Image.open(Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'assets/chick_character_design_pack/front_flap_source_v3.png').convert('RGBA')
folder=ROOT/'assets/runtime/chick/front_flap_patches'; folder.mkdir(parents=True,exist_ok=True)
cells=[]
for i in range(6):
    col,row=i%3,i//3
    cell=keep_largest_component(clean_alpha(sheet.crop((col*sheet.width//3,row*sheet.height//2,(col+1)*sheet.width//3,(row+1)*sheet.height//2))))
    cells.append(cell.crop(cell.getchannel('A').getbbox()))
scale=168/max(c.height for c in cells); includes=[]
for i,cell in enumerate(cells):
    cell=cell.resize((round(cell.width*scale),round(cell.height*scale)),Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(192,192)); canvas.alpha_composite(cell,((192-cell.width)//2,180-cell.height))
    raw=folder/f'aligned_{i}.png'; canvas.save(raw)
    eyes=re.findall(r'\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*0x',geometry(raw))
    eyes=[tuple(map(int,e)) for e in eyes]
    if len(eyes)!=2: raise ValueError('Expected two front eyes for alignment')
    ex=sum(x+w/2 for x,y,w,h in eyes)/2; ey=sum(y+h/2 for x,y,w,h in eyes)/2
    aligned=Image.new('RGBA',(192,192)); aligned.alpha_composite(canvas,(round(95.5-ex),round(79.5-ey)))
    aligned.save(raw)
    for side,x in [('left',10),('right',132)]:
        png=folder/f'{i}_{side}.png'; aligned.crop((x,80,x+50,148)).save(png)
        name=f'pet_front_flap_patch_{i}_{side}'
        write_c_image(png,ROOT/'lvgl-simulator/src'/f'{name}.c',name); includes.append(f'#include "{name}.c"')
(ROOT/'lvgl-simulator/src/pet_front_flap_patches.c').write_text('\n'.join(includes)+'\n')
