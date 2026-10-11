# Soft painted clouds

Generated with the built-in ImageGen tool from the user-approved cloud reference. Prompt: `prompt.txt`. Only cloud artwork is generated; the existing sky, meadow, plants and chick assets are unchanged.

`source.png` contains two different rounded cloud silhouettes with milk-white tops and softly pale-blue undersides. Near cloud is fuller, 148×72; distant cloud is smaller, 76×36, with lower runtime opacity. Soft alpha is preserved; no hard silhouette threshold or painted outline is added.

Pack using `lvgl-simulator/tools/prepare_cloud_sprites.py source.png assets/runtime/backgrounds/clouds_v2 lvgl-simulator/src/pet_cloud_assets.c --split 0.575` (paths relative to project root; source is `assets/backgrounds/clouds_v2/source.png`). The observed gutter is at 57.5%, not the sheet midpoint, so neither cloud is clipped.

Runtime replaces the old overlapping circle/rectangle cloud objects with independent bitmap layers. Left-to-right travel, the current 480,000 ms cycle and 33 ms timer remain unchanged. Only start phase and vertical placement are adjusted so both complete clouds are visible at launch. Wrap happens fully offscreen.

`scene_preview.png` is an actual offscreen LVGL scene rendering, not a generated whole-scene mockup.
