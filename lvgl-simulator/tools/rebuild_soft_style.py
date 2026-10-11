#!/usr/bin/env python3
"""Repack approved ImageGen soft-style assets without changing animation timings."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / 'lvgl-simulator/tools'
SOURCE = ROOT / 'assets/chick_character_design_pack/soft_style_v1'
CORRECTIONS = ROOT / 'assets/chick_character_design_pack/soft_style_v2'
RUNTIME = ROOT / 'assets/runtime/chick'
CODE = ROOT / 'lvgl-simulator/src'


def run(tool, *arguments):
    subprocess.run([sys.executable, str(TOOLS / tool), *map(str, arguments)], check=True)


def source(name):
    """Only explicitly corrected sheets override the previous approved set."""
    corrected = CORRECTIONS / name
    return corrected if corrected.exists() else SOURCE / name


def main():
    run('prepare_daily_sprites.py', source('daily_eyes.png'), RUNTIME / 'daily', CODE / 'pet_daily_sprites.c')
    run('prepare_side_mouth_bodies.py', SOURCE / 'side_clean.png', RUNTIME / 'side_mouth_bodies_v1', CODE / 'pet_side_mouth_bodies.c')
    run('prepare_beak_sprites.py', SOURCE / 'beak.png', RUNTIME / 'beak_v2', CODE / 'pet_beak_assets.c')
    run('prepare_foot_rig.py', SOURCE / 'feet.png', RUNTIME / 'walk_rig_v3', CODE / 'pet_walk_rig_assets.c')
    run('prepare_state_sprites.py', SOURCE / 'states.png', RUNTIME / 'states')
    for state in ('hungry', 'sleep'):
        run('generate_lvgl_argb8888.py', RUNTIME / 'states' / f'state_{state}.png',
            CODE / f'pet_chick_{state}.c', '--name', f'pet_chick_{state}')
    for tool, asset in [('rest_head', 'rest'), ('sky_head', 'sky'), ('ground_head', 'ground'),
                        ('wing_unified', 'wing'), ('front_flap_patches', 'front')]:
        run(f'prepare_{tool}.py', source(f'{asset}.png'))
    run('prepare_blink_geometry.py')
    run('prepare_mouth_geometry.py')


if __name__ == '__main__':
    main()
