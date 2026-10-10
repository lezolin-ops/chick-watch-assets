#!/usr/bin/env python3
"""Compose production pet sprites over proposed watch backgrounds for visual QA."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
BACKGROUND_DIR = ROOT / "assets/runtime/backgrounds"
SPRITE_DIR = ROOT / "assets/runtime/chick"
OUTPUT_DIR = ROOT / "assets/backgrounds/previews"

SCENES = (
    ("morning_meadow", "bg_morning_meadow_home_410x502.png", "sprite_front_192.png", (109, 135)),
    ("sunflower_walk", "bg_sunflower_garden_home_410x502.png", "walk/walk_1.png", (109, 135)),
    ("coop_sleep", "bg_coop_night_sleep_410x502.png", "states/state_sleep.png", (109, 230)),
    ("coop_feed", "bg_coop_day_feed_410x502.png", "states/state_hungry.png", (109, 230)),
)


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    rendered: list[Image.Image] = []
    for name, background_name, sprite_name, position in SCENES:
        background = Image.open(BACKGROUND_DIR / background_name).convert("RGBA")
        sprite = Image.open(SPRITE_DIR / sprite_name).convert("RGBA")
        background.alpha_composite(sprite, position)
        background.save(OUTPUT_DIR / f"{name}_preview_410x502.png")
        rendered.append(background.convert("RGB"))

    contact_sheet = Image.new("RGB", (820, 1004))
    for index, scene in enumerate(rendered):
        contact_sheet.paste(scene, ((index % 2) * 410, (index // 2) * 502))
    contact_sheet.save(OUTPUT_DIR / "scene_preview_contact_sheet.png")


if __name__ == "__main__":
    main()
