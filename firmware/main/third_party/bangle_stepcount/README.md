# Bangle.js step counter port

Upstream: https://github.com/espruino/Espruino
Pinned source revision: `2913ca0a7dd5d2e2ceb4d93e5f9a5ce3eca92b47`
Files: `libs/misc/stepcount.c`, `libs/misc/stepcount.h`.
Retrieved 2026-10-19. Downloaded C matched the pinned revision byte-for-byte.
Original C SHA256: `5bae49dd6377b175d6955187aa076f3481b10b8310501498ce1b76386977820a`.
Original header SHA256: `058a37dcd3a7d07d2fbb4b63b5d5d39b64d573393dd37eaba3c5c76e0a2a6cbb`.

MPL-2.0; original copyright notices retained; LICENSE accompanies the source.
Distributing the compiled port requires making the covered source and its
modifications available as required by that license.

Local changes to stepcount.c:
- Replace Espruino jsutils.h/int_sqrt32 dependency with standalone integer sqrt.
- Reset active_sample_count and gate_open in stepcount_init; upstream init does
  not clear these, which matters when resetting after an I/O/sampling gap.

Filter coefficients, activity gate, step timing, and six-step confirmation
parameters are unchanged. This is a singleton called by exactly one worker.
The external adapter converts QMI ±2g raw units to 8192/g, uses 64-bit square
accumulation, and rejects discontinuous/duplicate samples. No Espruino runtime
or JavaScript interpreter is linked.

See ../../../../docs/reference/software-steps-port-20261019.md for tests,
replay limitations, and device acceptance status. Third-party recordings are
not redistributed in this repository.
