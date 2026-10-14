#!/bin/sh
set -eu
power_root=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
cd "$power_root"
power_out=${WATCH_POWER_TEST_OUTPUT:-/private/tmp/mini-power-tests}
mkdir -p "$power_out"
clang -Wno-deprecated-declarations -DLV_CONF_INCLUDE_SIMPLE -DWATCH_CPU_MIN_MHZ=80 -DWATCH_CPU_MAX_MHZ=240 \
 -Ifirmware/tests/power/stubs -Ifirmware/tests/settings_ui/stubs -Ilvgl-simulator -Ilvgl-simulator/lvgl -Ifirmware/main \
 firmware/tests/power/policy.c lvgl-simulator/build/lvgl/liblvgl.a lvgl-simulator/build/lvgl/liblvgl_thorvg.a \
 -Ltools/sdl2-prefix/lib -lSDL2 -lpthread -lm -lc++ -o "$power_out/policy"
clang -Wno-deprecated-declarations -DLV_CONF_INCLUDE_SIMPLE \
 -Ifirmware/tests/power/stubs -Ilvgl-simulator -Ilvgl-simulator/lvgl -Ifirmware/main \
 firmware/tests/power/scene_pause.c lvgl-simulator/build/lvgl/liblvgl.a lvgl-simulator/build/lvgl/liblvgl_thorvg.a \
 -Ltools/sdl2-prefix/lib -lSDL2 -lpthread -lm -lc++ -o "$power_out/scene_pause"
DYLD_LIBRARY_PATH="$power_root/tools/sdl2-prefix/lib" "$power_out/policy"
DYLD_LIBRARY_PATH="$power_root/tools/sdl2-prefix/lib" "$power_out/scene_pause"
