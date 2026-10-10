# 模拟器源码导入

来源：https://github.com/hiJoeLee/chick-watch-simulator
原始提交：7d822702f54cb91b00935c22f251b5aeecaabb80

模拟器与根目录 assets/ 共同组成电子宠物项目。源码保持原样，LVGL 与 FreeRTOS 保留原始固定提交版本。

需要 C/C++ 编译器、CMake、SDL2 开发库。图片工具另外需要 Python 和 Pillow。

从仓库根目录执行：

```bash
git submodule update --init --recursive
cmake -S lvgl-simulator -B lvgl-simulator/build
cmake --build lvgl-simulator/build -j2
./lvgl-simulator/bin/main
```

本次完成源码迁移，尚未验证编译与运行。
