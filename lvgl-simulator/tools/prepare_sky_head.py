"""Package sky-looking head frames on the original fixed seated lower body."""
from pathlib import Path
import sys
from PIL import Image
from prepare_state_sprites import clean_alpha,keep_largest_component
from generate_lvgl_argb8888 import write_c_image
from prepare_blink_geometry import geometry
ROOT=Path(__file__).resolve().parents[2]
folder=ROOT/'assets/runtime/chick/sky_head'
folder.mkdir(parents=True,exist_ok=True)
source=Image.open(Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'assets/chick_character_design_pack/sky_head_source_v1.png').convert('RGBA')
base=Image.open(ROOT/'assets/runtime/chick/daily/right_0.png').convert('RGBA')
bounds=base.getchannel('A').getbbox()
mask=Image.new('L',(192,192),0)
for y in range(150):
    mask.paste(255 if y<140 else round(255*(150-y)/10),(0,y,192,y+1))
includes=[]; eyes=[]
for i in range(3):
    cell=keep_largest_component(clean_alpha(source.crop((i*source.width//3,0,(i+1)*source.width//3,source.height))))
    cell=cell.crop(cell.getchannel('A').getbbox())
    cell=cell.resize((bounds[2]-bounds[0],bounds[3]-bounds[1]),Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(192,192)); canvas.alpha_composite(cell,bounds[:2])
    frame=Image.composite(canvas,base,mask)
    png=folder/f'{i}.png'; frame.save(png)
    name=f'pet_sky_head_{i}'
    write_c_image(png,ROOT/'lvgl-simulator/src'/f'{name}.c',name)
    includes.append(f'#include "{name}.c"'); eyes.append(geometry(png))
(ROOT/'lvgl-simulator/src/pet_sky_head_assets.c').write_text('\n'.join(includes)+'\n')
(ROOT/'lvgl-simulator/src/pet_sky_head_eyes.h').write_text('static const eye_pose_t sky_head_eyes[3] = {\n'+'\n'.join('    '+e+',' for e in eyes)+'\n};\n')
