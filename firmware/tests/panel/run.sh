#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
clang -Wno-deprecated-declarations -DLV_CONF_INCLUDE_SIMPLE -Ifirmware/tests/panel/stubs -Ilvgl-simulator -Ilvgl-simulator/lvgl -Ifirmware/main firmware/tests/panel/test.c lvgl-simulator/build/lvgl/liblvgl.a lvgl-simulator/build/lvgl/liblvgl_thorvg.a -Ltools/sdl2-prefix/lib -lSDL2 -lpthread -lm -lc++ -o /private/tmp/mini-panel-test
DYLD_LIBRARY_PATH="$PWD/tools/sdl2-prefix/lib" /private/tmp/mini-panel-test
