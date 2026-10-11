# Selective yellow palette correction

The unchanged v1 `side_clean.png` is the approved yellow material reference.

Only three source sheets override v1: `daily_eyes.png` (daily/front action poses), `sky.png` (upward observation), and `wing.png` (independent wing frames). These had a visibly darker golden feather material. All other authored source sheets, including side bodies, beaks, feet, yaw, downward observation, front flap patches and expression states remain v1.

Generated with the built-in ImageGen image editor. Exact prompts are saved in `prompts.json`. Original v1 images are preserved. Animation timings and scene are unchanged.

Head packers still reuse the current seated lower body to avoid mixed colours on shared belly/feet; that mechanical registration does not replace the original yaw/downward head artwork.

`lvgl-simulator/tools/rebuild_soft_style.py` automatically uses only these three v2 overrides and v1 for everything else.
