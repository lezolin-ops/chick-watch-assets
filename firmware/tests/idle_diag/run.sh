#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
clang -Wall -Wextra -Werror -Ifirmware/tests/idle_diag/stubs -Ifirmware/main firmware/tests/idle_diag/test.c -o /private/tmp/mini-idle-diag-test
/private/tmp/mini-idle-diag-test
