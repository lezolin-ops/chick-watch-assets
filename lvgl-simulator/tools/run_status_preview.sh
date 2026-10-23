#!/bin/sh
set -eu
preview_root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$preview_root"
preview_bin=${TMPDIR:-/private/tmp}/mini-status-preview
clang -Wno-deprecated-declarations -DLV_CONF_INCLUDE_SIMPLE -DPET_USE_PACKED_ASSETS=0 \
 -Ifirmware/tests/settings_ui/stubs -Ilvgl-simulator -Ilvgl-simulator/lvgl -Ifirmware/main -Itools/sdl2-prefix/include/SDL2 \
 lvgl-simulator/tools/status_preview.c firmware/main/pet.c firmware/main/watch_home_clock.c \
 firmware/main/watch_step_numbers_30.c firmware/main/watch_pill_numbers_24.c firmware/main/watch_date_26.c \
 firmware/main/watch_steps_font_24.c firmware/main/watch_font_24.c firmware/main/watch_cartoon_68.c \
 lvgl-simulator/build/lvgl/liblvgl.a lvgl-simulator/build/lvgl/liblvgl_thorvg.a \
 -Ltools/sdl2-prefix/lib -Wl,-rpath,"$preview_root/tools/sdl2-prefix/lib" -lSDL2 -lpthread -lm -lc++ -o "$preview_bin"
[ "${PREVIEW_BUILD_ONLY:-0}" = 1 ] && exit 0
DYLD_LIBRARY_PATH="$preview_root/tools/sdl2-prefix/lib" exec "$preview_bin"
