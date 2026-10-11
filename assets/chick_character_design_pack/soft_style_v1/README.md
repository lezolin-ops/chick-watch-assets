# Approved soft material set

Generated with the built-in ImageGen editing tool from original registered pose sheets.

Keeps the current silhouette, warm yellow, pink cheeks and matte orange beak. Hard perimeter strokes are replaced with restrained soft shading. Eyes retain the main white reflection and a subdued warm-brown secondary reflection. No lashes, glossy beak or per-pose tint is added.

Active sheets: `daily_eyes.png`, `side_clean.png`, `rest.png`, `sky.png`, `ground.png`, `wing.png`, `front.png`, `beak.png`, `feet.png`, `states.png`. Earlier `daily.png` and `side.png` remain as iterations. Original sheets in the parent directory are unchanged.

Rebuild using `lvgl-simulator/tools/rebuild_soft_style.py` with a Python environment containing Pillow. It performs atlas extraction, registration, resizing and runtime packaging. Appearance edits were performed with ImageGen.

Side bodies have an authored wingless interior; the independently animated wing is the only moving feather part. The rump remains unchanged. Runtime no longer paints an orange wing outline or recolours only the side beak.

Current state-machine, cloud, idle timing and click-interaction changes are preserved. This migration changes materials and registration, not behaviour.

Prompts: `prompts.json`, `component_prompts.json`, `refinement_prompts.json`.
