#!/usr/bin/env python3
"""Locate eyes for native LVGL eyelids; never rescale the chick to blink."""
from collections import deque
from pathlib import Path
from statistics import median
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


def geometry(path):
    image = Image.open(path).convert('RGBA')
    pixels = image.load()
    candidates = {(x, y) for y in range(150) for x in range(192)
                  if pixels[x, y][3] > 128 and pixels[x, y][0] < 85
                  and pixels[x, y][1] < 75 and pixels[x, y][2] < 65}
    components = []
    while candidates:
        start = candidates.pop()
        queue = deque([start])
        component = [start]
        while queue:
            x, y = queue.popleft()
            for point in [(x-1,y), (x+1,y), (x,y-1), (x,y+1)]:
                if point in candidates:
                    candidates.remove(point)
                    queue.append(point)
                    component.append(point)
        if len(component) > 12:
            components.append(component)
    eyes = []
    for component in sorted(components, key=len, reverse=True)[:2]:
        x0, x1 = min(p[0] for p in component), max(p[0] for p in component)
        y0, y1 = min(p[1] for p in component), max(p[1] for p in component)
        # Feather color around each eye, rather than a generic yellow patch.
        colors = [pixels[x, y][:3] for y in range(y0-3, y1+4) for x in range(x0-3, x1+4)
                  if (x < x0-1 or x > x1+1 or y < y0-1 or y > y1+1)
                  and pixels[x, y][3] > 128 and pixels[x,y][0] > 190 and pixels[x,y][1] > 140]
        color = [int(median(c[k] for c in colors)) for k in range(3)]
        eyes.append((x0-1, y0-1, x1-x0+3, y1-y0+3, (color[0]<<16)|(color[1]<<8)|color[2]))
    return '{ %d, { %s } }' % (len(eyes), ', '.join('{ %d, %d, %d, %d, 0x%06X }' % e for e in eyes))


def main():
    lines = ['/* Generated eye geometry for native LVGL eyelids. */',
             'typedef struct { int16_t x, y, w, h; uint32_t color; } eye_region_t;',
             'typedef struct { uint8_t count; eye_region_t eyes[2]; } eye_pose_t;']
    for name, folder, count in [('daily_eyes', 'daily', 12), ('illustrated_walk_eyes', 'walk_illustrated_v2', 4)]:
        lines.append(f'static const eye_pose_t {name}[{count}] = {{')
        lines.extend('    ' + geometry(ROOT / 'assets/runtime/chick' / folder / f'right_{i}.png') + ',' for i in range(count))
        lines.append('};')
    lines.append('static const eye_pose_t hungry_eyes = ' + geometry(ROOT / 'assets/runtime/chick/states/state_hungry.png') + ';')
    lines.append('static const eye_pose_t rig_body_eyes = ' + geometry(ROOT / 'assets/runtime/chick/walk_rig_v3/body_right.png') + ';')
    (ROOT / 'lvgl-simulator/src/pet_blink_geometry.h').write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
