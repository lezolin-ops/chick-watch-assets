"""Package ImageGen-separated body and wing for the native skeletal rig."""
from pathlib import Path
from PIL import Image,ImageOps
from prepare_state_sprites import clean_alpha,keep_largest_component
from generate_lvgl_argb8888 import write_c_image
ROOT=Path(__file__).resolve().parents[2]
sheet=Image.open(ROOT/'assets/chick_character_design_pack/wing_parts_source_v1.png').convert('RGBA')
cells=[]
for i in range(2):
    cell=keep_largest_component(clean_alpha(sheet.crop((i*sheet.width//2,0,(i+1)*sheet.width//2,sheet.height))))
    cells.append(cell.crop(cell.getchannel('A').getbbox()))
body,wing=cells
scale=144/body.height
body=body.resize((round(body.width*scale),144),Image.Resampling.LANCZOS)
canvas=Image.new('RGBA',(192,192))
canvas.alpha_composite(body,(165-body.width,18))
wing=wing.resize((50,42),Image.Resampling.LANCZOS)
folder=ROOT/'assets/runtime/chick/wing_parts'
folder.mkdir(parents=True,exist_ok=True)
includes=[]
for key,im in [('body',canvas),('wing',wing)]:
    for direction,frame in [('right',im),('left',ImageOps.mirror(im))]:
        name=f'pet_wing_{key}_{direction}'
        png=folder/f'{key}_{direction}.png'
        frame.save(png)
        write_c_image(png,ROOT/'lvgl-simulator/src'/f'{name}.c',name)
        includes.append(f'#include "{name}.c"')
(ROOT/'lvgl-simulator/src/pet_wing_parts.c').write_text('\n'.join(includes)+'\n')
