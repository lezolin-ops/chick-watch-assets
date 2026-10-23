#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
out=${TMPDIR:-/private/tmp}/mini-steps-test
clang -Wall -Wextra -Werror -Ifirmware/tests/steps/stubs -Ifirmware/main firmware/tests/steps/test.c firmware/main/watch_step_algorithm.c firmware/main/third_party/bangle_stepcount/stepcount.c -lm -o "$out"
"$out"
clang -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all -Ifirmware/main \
 firmware/tests/steps/algorithm.c firmware/main/watch_step_algorithm.c \
 firmware/main/third_party/bangle_stepcount/stepcount.c -lm -o "$out-algorithm"
"$out-algorithm"
python3 firmware/tests/steps/replay.py
