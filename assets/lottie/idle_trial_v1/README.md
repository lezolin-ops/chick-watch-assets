# Five-second parameter idle trial

Standalone vector/Lottie study; current pet scene and simulator entry point are not modified.

- 480×480, 30fps, 150 frames, exactly five seconds.
- Breathing: 100→102.5→100%, approximately one-second cycles, gentle half-amplitude entry and exit.
- Feet remain fixed. Body, face, tuft and wing shoulders share one breathing parent anchored near the belly base.
- Blink: frames45→47→49, at 1.5 seconds, vertical eye scale100→10→100%.
- Screen-left wing: frames84→99→114, one12° upward flap from2.8–3.8s. Positive rotation raises a leftward-facing shape in this coordinate system. The reference diagram's2.8–4.2s label does not match its84–114 frame specification; this trial follows the explicit frame specification.
- Mouth stays a quiet open smile, no extra chirps or autonomous tuft motion.
- End pose equals start pose; no new actions during the final settling beat.

`idle.json` is editable animation data. `preview.gif` is rendered by the project's actual bundled ThorVG engine, not AI-generated video. `motion_review.png` shows key poses. GIF timing uses30/30/40ms frame delays to retain exactly5000ms despite GIF's10ms timing granularity.

Rebuild: run `lvgl-simulator/tools/generate_idle_trial.py`, compile/run `render_idle_trial.c` linked against the simulator's LVGL and ThorVG libraries, then run `export_idle_trial.py`.
