"""Pack six hand-drawn wings with one anatomical scale and shoulder anchor."""
from pathlib import Path
from PIL import Image,ImageOps
from prepare_state_sprites import clean_alpha,keep_largest_component
from generate_lvgl_argb8888 import write_c_image
ROOT=Path(__file__).resolve().parents[2]
sheet=Image.open(ROOT/'assets/chick_character_design_pack/wing_frames_source_v1.png').convert('RGBA')
cells=[]
for i in range(6):
    col,row=i%3,i//3
    cell=keep_largest_component(clean_alpha(sheet.crop((col*sheet.width//3,row*sheet.height//2,(col+1)*sheet.width//3,(row+1)*sheet.height//2))))
    bounds=cell.getchannel('A').getbbox()
    if not bounds or bounds[2]-bounds[0]>sheet.width//3-8:
        raise ValueError('Wing must be an isolated transparent component')
    cells.append(cell.crop(bounds))
scale=min(42/max(c.width for c in cells),50/max(c.height for c in cells))
folder=ROOT/'assets/runtime/chick/wing_frames'
folder.mkdir(parents=True,exist_ok=True)
includes=[]
for i,cell in enumerate(cells):
    cell=cell.resize((round(cell.width*scale),round(cell.height*scale)),Image.Resampling.LANCZOS)
    # The narrow rightmost cap is the shoulder, never the feather tips.
    alpha=cell.getchannel('A')
    points=[(x,y) for x in range(cell.width-3,cell.width) for y in range(cell.height) if alpha.getpixel((x,y))>160]
    if not points: raise ValueError('Missing shoulder cap')
    root_x=round(sum(x for x,y in points)/len(points))
    root_y=round(sum(y for x,y in points)/len(points))
    canvas=Image.new('RGBA',(64,64))
    canvas.alpha_composite(cell,(54-root_x,34-root_y))
    for direction,frame in [('right',canvas),('left',ImageOps.mirror(canvas))]:
        name=f'pet_wing_frame_{i}_{direction}'
        png=folder/f'{i}_{direction}.png'; frame.save(png)
        write_c_image(png,ROOT/'lvgl-simulator/src'/f'{name}.c',name)
        includes.append(f'#include "{name}.c"')
(ROOT/'lvgl-simulator/src/pet_wing_frames.c').write_text('\n'.join(includes)+'\n')
