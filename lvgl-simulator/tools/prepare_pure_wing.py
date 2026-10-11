"""Package only the corrected isolated wing; never regenerate a body layer."""
from pathlib import Path
from PIL import Image,ImageOps
from prepare_state_sprites import clean_alpha,keep_largest_component
from generate_lvgl_argb8888 import write_c_image
ROOT=Path(__file__).resolve().parents[2]
im=Image.open(ROOT/'assets/chick_character_design_pack/pure_wing_source_v2.png').convert('RGBA')
im=keep_largest_component(clean_alpha(im))
im=im.crop(im.getchannel('A').getbbox()).resize((42,34),Image.Resampling.LANCZOS)
folder=ROOT/'assets/runtime/chick/wing_parts'
includes=[]
for direction,frame in [('right',im),('left',ImageOps.mirror(im))]:
    name=f'pet_wing_wing_{direction}'
    png=folder/f'wing_{direction}.png'
    frame.save(png)
    write_c_image(png,ROOT/'lvgl-simulator/src'/f'{name}.c',name)
    includes.append(f'#include "{name}.c"')
(ROOT/'lvgl-simulator/src/pet_wing_parts.c').write_text('\n'.join(includes)+'\n')
