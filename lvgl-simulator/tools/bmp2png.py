"""Convert BMP frames dumped by `main.exe --shot` to PNG for review.

Usage: python bmp2png.py <dir-or-prefix> ...
Example: python bmp2png.py E:/Projects/tools/offline/f_000.bmp
"""
import sys
import glob
import os
from PIL import Image

converted = 0
for arg in sys.argv[1:]:
    pattern = arg if any(c in arg for c in "*?") else os.path.splitext(arg)[0] + "_*.bmp"
    for f in glob.glob(pattern):
        if f.lower().endswith(".bmp"):
            out = os.path.splitext(f)[0] + ".png"
            Image.open(f).save(out)
            converted += 1
print(f"converted {converted} bmp -> png")
