#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
test_output=${WATCH_TEST_OUTPUT:-/private/tmp/mini-settings-test}
mkdir -p "$test_output"
cd "$task_root"
clang -DLV_CONF_INCLUDE_SIMPLE -Ifirmware/tests/settings_ui/stubs -Ilvgl-simulator -Ilvgl-simulator/lvgl -Ifirmware/main \
  firmware/tests/settings_ui/test.c firmware/main/watch_font_24.c firmware/main/watch_cartoon_68.c \
  lvgl-simulator/build/lvgl/liblvgl.a lvgl-simulator/build/lvgl/liblvgl_thorvg.a \
  -Ltools/sdl2-prefix/lib -lSDL2 -lpthread -lm -lc++ -o "$test_output/test"
WATCH_TEST_OUTPUT="$test_output" DYLD_LIBRARY_PATH="$task_root/tools/sdl2-prefix/lib" "$test_output/test"
