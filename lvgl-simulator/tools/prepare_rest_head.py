"""Package generated head turns against the unchanged seated lower-body layer."""
from pathlib import Path
from PIL import Image, ImageOps
from prepare_state_sprites import clean_alpha, keep_largest_component
from generate_lvgl_argb8888 import write_c_image
from prepare_blink_geometry import geometry

ROOT=Path(__file__).resolve().parents[2]
folder=ROOT/'assets/runtime/chick/rest_head'
folder.mkdir(parents=True,exist_ok=True)
source=Image.open(ROOT/'assets/chick_character_design_pack/rest_head_source_v1.png').convert('RGBA')
base=Image.open(ROOT/'assets/runtime/chick/daily/right_0.png').convert('RGBA')
bounds=base.getchannel('A').getbbox()
includes=[]
eyes=[]
for i in range(3):
    cell=source.crop((i*source.width//3,0,(i+1)*source.width//3,source.height))
    cell=keep_largest_component(clean_alpha(cell))
    cell=cell.crop(cell.getchannel('A').getbbox())
    cell=cell.resize((bounds[2]-bounds[0],bounds[3]-bounds[1]),Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(192,192))
    canvas.alpha_composite(cell,bounds[:2])
    for direction,head in [('right',canvas),('left',ImageOps.mirror(canvas))]:
        # Original belly, wings and tucked feet are shared by every head frame.
        mask=Image.new('L',(192,192),0)
        for y in range(150):
            opacity=255 if y<140 else round(255*(150-y)/10)
            mask.paste(opacity,(0,y,192,y+1))
        frame=Image.composite(head,base,mask)
        png=folder/f'{i}_{direction}.png'
        frame.save(png)
        name=f'pet_rest_head_{i}_{direction}'
        write_c_image(png,ROOT/'lvgl-simulator/src'/f'{name}.c',name)
        includes.append(f'#include "{name}.c"')
    eyes.append(geometry(folder/f'{i}_right.png'))
(ROOT/'lvgl-simulator/src/pet_rest_head_assets.c').write_text('\n'.join(includes)+'\n')
(ROOT/'lvgl-simulator/src/pet_rest_head_eyes.h').write_text('static const eye_pose_t rest_head_eyes[3] = {\n'+ '\n'.join('    '+e+',' for e in eyes)+'\n};\n')
