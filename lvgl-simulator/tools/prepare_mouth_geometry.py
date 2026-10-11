#!/usr/bin/env python3
"""Locate full beak silhouettes for independently drawn upper/lower bills."""
from collections import deque
from pathlib import Path
from statistics import median
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
# Whole beak editing bounds, attachment point and viewing angle.
POSES = [(80,109,112,138,95,121,0), (80,97,112,127,95,109,0),
         (80,80,112,108,95,92,0), (80,73,112,103,95,86,0),
         (102,78,134,107,117,90,1), (123,77,153,109,137,90,1),
         (138,80,170,114,144,94,2), (138,80,170,114,145,94,2),
         (139,80,171,114,146,94,2), (139,80,171,114,146,94,2),
         (103,78,136,107,118,90,1), (79,116,112,146,95,128,0)]


def geometry(path, pose):
    im = Image.open(path).convert('RGBA')
    p = im.load()
    x0,y0,x1,y1,cx,cy,mode = pose
    candidates = {(x,y) for y in range(y0,y1) for x in range(x0,x1)
                  if (lambda c:c[3]>128 and c[0]>80 and c[1]<180 and c[2]<100)(p[x,y])}
    parts = []
    while candidates:
        start = candidates.pop()
        q, part = deque([start]), [start]
        while q:
            x,y = q.popleft()
            for n in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
                if n in candidates:
                    candidates.remove(n); q.append(n); part.append(n)
        if len(part)>20: parts.append(part)
    if not parts: return '{ 0 }'
    part = max(parts,key=len)
    # Include the old beak's anti-aliased fringe, avoiding lingering orange dots.
    expanded = {(x+dx,y+dy) for x,y in part for dx in (-1,0,1) for dy in (-1,0,1)
                if x0<=x+dx<x1 and y0<=y+dy<y1}
    part = list(expanded)
    rows = []
    top_edge = max(x for x in range(192) if p[x,y0-1][3]>128)
    bottom_edge = max(x for x in range(192) if p[x,y1][3]>128)
    fallback = (252,214,54)
    for y in (range(y0,y1) if mode==2 else sorted({y for x,y in part})):
        xs = [x0,x1-1] if mode==2 else [x for x,yy in part if yy==y]
        t = (y-y0+1)/(y1-y0+1)
        smooth = t*t*(3-2*t)
        edge = round(top_edge+(bottom_edge-top_edge)*smooth) if mode==2 else 191
        colors = [p[x,y][:3] for x in range(max(0,x0-12),x0+2)
                  if p[x,y][3]>128 and p[x,y][0]>220 and p[x,y][1]>180 and p[x,y][2]<145]
        color = [round(median(c[k] for c in colors)) for k in range(3)] if colors else fallback
        rgb = (color[0]<<16)|(color[1]<<8)|color[2]
        rows.append('{ %d, %d, %d, %d, 0x%06X }' % (y,min(xs),max(xs),edge,rgb))
    return '{ %d,%d,%d,%d,%d,%d,%d,%d, { %s } }' % (x0,y0,x1-x0,y1-y0,len(rows),mode,cx,cy,', '.join(rows))


def main():
    lines = ['/* Full-beak masks and face restoration rows; original art stays intact. */',
             'typedef struct { uint8_t y,x1,x2,skin_x2; uint32_t color; } mouth_row_t;',
             'typedef struct { uint8_t x,y,w,h,count,mode,cx,cy; mouth_row_t rows[48]; } mouth_pose_t;',
             'static const mouth_pose_t daily_mouths[12] = {']
    lines.extend('    '+geometry(ROOT/'assets/runtime/chick/daily'/f'right_{i}.png',pose)+',' for i,pose in enumerate(POSES))
    lines.append('};')
    lines.append('static const mouth_pose_t rig_body_mouth = '+geometry(ROOT/'assets/runtime/chick/walk_rig_v3/body_right.png',(139,72,170,114,144,93,2))+';')
    (ROOT/'lvgl-simulator/src/pet_mouth_geometry.h').write_text('\n'.join(lines)+'\n')


if __name__=='__main__': main()
