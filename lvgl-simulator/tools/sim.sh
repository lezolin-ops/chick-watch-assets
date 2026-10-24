#!/usr/bin/env bash
# ============================================================
#  chick-watch 模拟器助手（Git Bash 专用）
#
#  用法（先 cd 到本目录，或在任意处用绝对路径调用）：
#    ./sim.sh build                 编译（增量）
#    ./sim.sh run                   启动模拟器窗口
#    ./sim.sh shot happy 80 2       导出 happy 动画 80 帧（每 2x33ms）
#    ./sim.sh shot angry 120 2
#    ./sim.sh shot sleep 100 3
#    ./sim.sh cd                    打印项目目录（配合 eval cd "$(./sim.sh cd)"）
#
#  说明：本脚本只需解决两件事 —— 工具链 PATH 与工作目录。
# ============================================================
set -u

SIM="E:/Projects/chick-watch-assets/lvgl-simulator"
MINGW="/c/Users/jonyl/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
SDLBIN="E:/Projects/tools/SDL2-2.32.10/x86_64-w64-mingw32/bin"
CMAKEBIN="/c/Program Files/CMake/bin"
PY="C:/Users/jonyl/.workbuddy/binaries/python/envs/default/Scripts/python.exe"

export PATH="$MINGW:$SDLBIN:$CMAKEBIN:$PATH"
cd "$SIM" || { echo "项目目录不存在: $SIM"; exit 1; }

cmd="${1:-run}"
case "$cmd" in
  cd)
    echo "$SIM"
    ;;
  build)
    cmake --build build -j 8
    ;;
  run)
    ./bin/main.exe
    ;;
  shot)
    demo="${2:-happy}"
    count="${3:-80}"
    every="${4:-2}"
    out="E:/Projects/tools/frames_${demo}"
    mkdir -p "$out"
    rm -f "$out"/f_*.bmp "$out"/f_*.png
    echo "渲染中: $demo, $count 帧, 间隔 $((every*33))ms ..."
    ./bin/main.exe --"${demo}"-demo --shot "$count" "$every" "$out/f"
    "$PY" "$SIM/tools/bmp2png.py" "$out/f"
    echo "完成 -> $out"
    ;;
  *)
    echo "未知命令: $cmd"
    echo "可用: build | run | shot <happy|angry|sleep> [帧数] [间隔] | cd"
    exit 1
    ;;
esac
