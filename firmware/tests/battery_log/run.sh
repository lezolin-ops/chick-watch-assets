#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
test_binary=$(mktemp /private/tmp/mini-battery-format.XXXXXX)
trap 'rm -f "$test_binary"' EXIT
clang -std=c11 -Wall -Wextra -Werror -I firmware/main firmware/tests/battery_log/format.c -o "$test_binary"
"$test_binary"
python3 - <<'PY'
import runpy
parse=runpy.run_path('firmware/tools/export_battery_log.py')['parse']
s='BATLOG_BEGIN,1\nBATLOG,2,0,180,4100,100,1,2,0,0,0,0\nBATLOG,1,0,0,4200,100,1,0,50,0,0,0\nBATLOG_END\n'
assert [r['seq'] for r in parse(s)]==[1,2]
try:parse(s.replace('BATLOG_END',''))
except ValueError:pass
else:raise AssertionError('accepted partial export')
print('PASS: CRC, bounds, export ordering and incomplete-export rejection')
PY
