"""Build a small LVGL 4bpp Chinese subset from a local font, with Latin fallback."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

p = argparse.ArgumentParser()
p.add_argument('--font', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
chars = sorted(set('0123456789:-'))
font = ImageFont.truetype(a.font, 88)
bitmap = []
descriptors = ['{0}']
for char in chars:
    x0, y0, x1, y1 = font.getbbox(char, anchor='ls', stroke_width=2)
    w, h = x1-x0, y1-y0
    image = Image.new('L', (max(1,w), max(1,h)))
    ImageDraw.Draw(image).text((-x0,-y0), char, font=font, fill=255, anchor='ls', stroke_width=2)
    pixels = [v >> 4 for v in image.getdata()]
    if len(pixels) % 2: pixels.append(0)
    descriptors.append('{.bitmap_index=%d,.adv_w=%d,.box_w=%d,.box_h=%d,.ofs_x=%d,.ofs_y=%d}' %
        (len(bitmap),round((font.getlength(char)+4)*16),w,h,x0,-y1))
    bitmap.extend((pixels[i]<<4)|pixels[i+1] for i in range(0,len(pixels),2))
start = ord(chars[0])
result = '#include "lvgl.h"\n/* Generated numeral subset: run firmware/tools/generate_cartoon_font.py. */\n'
result += 'static const uint8_t bitmap[] = {\n' + ',\n'.join(','.join(map(str,bitmap[i:i+24])) for i in range(0,len(bitmap),24)) + '};\n'
result += 'static const lv_font_fmt_txt_glyph_dsc_t glyphs[] = {\n' + ',\n'.join(descriptors) + '};\n'
result += 'static const uint16_t unicode[] = {' + ','.join(str(ord(c)-start) for c in chars) + '};\n'
result += 'static const lv_font_fmt_txt_cmap_t cmap[] = {{.range_start=%d,.range_length=%d,.glyph_id_start=1,.unicode_list=unicode,.list_length=%d,.type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}};\n' % (start,ord(chars[-1])-start+1,len(chars))
result += 'static const lv_font_fmt_txt_dsc_t dsc = {.glyph_bitmap=bitmap,.glyph_dsc=glyphs,.cmaps=cmap,.cmap_num=1,.bpp=4};\n'
result += 'const lv_font_t watch_cartoon_68 = {.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height=100,.base_line=18,.dsc=&dsc};\n'
Path(a.output).write_text(result)
print(f'{len(chars)} glyphs, {len(bitmap)} bitmap bytes')
