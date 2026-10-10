# VSCode Simulator project for LVGL

[LVGL](https://github.com/lvgl/lvgl) is written mainly for microcontrollers and embedded systems, however you can run the library **on your PC** as well without any embedded hardware. The code written on PC can be simply copied when your are using an embedded system.

This project is pre-configured for VSCode and should work work on Windows, Linux and MacOs as well. FreeRTOS is also included and can be optionally enabled to better simulate embedded system's behavior. 

## 电子宠物当前配置

- 模拟器画布：410 × 502，与微雪 2.06 英寸 AMOLED 的目标分辨率一致。
- 宠物素材：`src/pet_chick_192.c`，由角色设定包的透明主立绘导出；走路、饥饿与睡觉贴图均位于 `assets/runtime/chick/`。
- 宠物入口：`src/pet.c`；包含待机、四帧走路、开心弹跳、饥饿、睡觉；待机保留脚底锚定的轻呼吸，加入独立眼皮眨眼，取消整身压缩式假眨眼和大幅心跳缩放。
- 默认保持待机，脚底固定并带接地阴影。点击小鸡不透明部分触发约 0.5 秒、18 像素的轻跳，结束后恢复待机；动作中重复点击不会叠加。走路、饥饿和睡眠仍可通过 pet_set_state 切换。
- 运行贴图转换工具：`tools/prepare_chick_sprite.py`、`tools/prepare_walk_sprites.py`、`tools/prepare_state_sprites.py` 和 `tools/generate_lvgl_argb8888.py`。

## Get started

### Install SDL and the build tools

- **Windows (vcpkg):** `vcpkg install sdl2`  (`vcpkg` can be installed from [https://github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg)) Also install either MinGW or another compiler and `cmake`.
- **macOS (Homebrew):** `brew install sdl2 cmake make`  
- **Linux:**  
  - **Debian/Ubuntu:** `sudo apt install build-essential cmake libsdl2-dev`  
  - **Arch:** `sudo pacman -S base-devel cmake sdl2`  
  - **Fedora:** `sudo dnf install @development-tools cmake SDL2-devel`  
- **Manual Installation of SDL:** Download from [SDL’s website](https://github.com/libsdl-org/SDL/releases) and place headers/libraries in your project.
- **Verify Installation:** `sdl2-config --version`, `cmake --version`, `gcc --version`, `g++ --version` (should return the installed version).  

### Get the PC project

Clone the PC project and the related sub modules:

```bash
git clone --recursive https://github.com/lvgl/lv_port_pc_vscode
```

## Usage

### Visual Studio Code

1. Be sure you have installed [SDL and the build tools](#install-sdl-and-the-build-tools)
2. Open the project by double clicking on `simulator.code-workspace` or opening it with `File/Open Workspace from File`
3. Install the recommended plugins
4. Click the Run and Debug page on the left, and select `Debug LVGL demo with gdb` from the drop-down on the top. Like this:
![image](https://github.com/lvgl/lv_port_pc_vscode/assets/7599318/f527b235-5718-4949-b5f0-bd807b3a64ba)
5. Click the Play button or hit F5 to start debugging.

#### ArchLinux User

VSCode does not officially provide an installation package under Arch, you need to use the AUR manager `paru` to install it.
The command is as follows:

```bash
paru -S visual-studio-code-bin
```

#### macOS

Apple's default clang does not support the `-fsanitize=leak` flag.

to build using the latest version of clang from homebrew, do the following:

1. `brew install llvm`

2. cmd+shift+p and run `Cmake: select a kit`, then `[Scan for kits]`

3. then cmd+shift+p and run `Cmake: select a kit`, select the version of clang you just installed from homebrew (it should say `Using compilers C=/opt/homebrew/opt/llvm/bin/clang ...`)

4. reconfigure by running cmd+shift+p `Cmake: Configure`

5. build using [step 4 above](#visual-studio-code)

### FreeRTOS configuration
To correctly configure the project, the RTOS (Real-Time Operating System) requires a significant amount of heap memory, especially when debugging an SDL (Simple DirectMedia Layer) window application. In this project, the heap memory has been experimentally set to **512 MB**.

```c
#define configTOTAL_HEAP_SIZE ( ( size_t ) ( 512 * 1024 * 1024 ) )  // 512 MB Heap
```
This configuration ensures that the SDL window is displayed in a timely manner. If this value is reduced, it may cause significant delays in the SDL window's appearance. If the allocated heap memory is too small, the window may fail to appear altogether.
Therefore, it is crucial to allocate sufficient heap memory to ensure smooth execution and debugging experience.

### Enable FreeRTOS 
To enable the rtos part of this project select in lv_conf.h `#define LV_USE_OS   LV_OS_NONE` to `#define LV_USE_OS  LV_OS_FREERTOS`
Additionaly you have to enable the compilation of all FreeRTOS Files by turning on the `option(USE_FREERTOS "Enable FreeRTOS" OFF)` in the CMakeLists.txt file or
by enabling the same flag from the command line when bootstrapping `cmake`:

```bash
cmake -B build -DUSE_FREERTOS=ON
```

### CMake

This project uses CMake under the hood which can be used without Visula Studio Code too. Just type these in a Terminal when you are in the project's root folder:

```bash
mkdir build
cd build
cmake ..
make -j
```

## Run demos and examples

By default, the widgets demo (`lv_demo_widgets()`) will run. If you want to run a different demo or example from the LVGL library,
simply replace the demo function call in the code with another one—such as `lv_demo_benchmark()` or `lv_example_label_1()`.

```c
int main(int argc, char **argv)
{
  /* ... */
  /* Run the default demo */
  /* To try a different demo or example, replace this with one of: */
  /* - lv_demo_benchmark(); */
  /* - lv_demo_stress(); */
  /* - lv_example_label_1(); */
  /* - etc. */
  lv_demo_widgets(); 

  while(1) {
      /* ... */
  }
  return 0;
}
```

## Optional library

There are also FreeType and FFmpeg support. You can install these according to the followings:

### Linux

```bash
# FreeType support
wget https://kumisystems.dl.sourceforge.net/project/freetype/freetype2/2.13.2/freetype-2.13.2.tar.xz
tar -xf freetype-2.13.2.tar.xz
cd freetype-2.13.2
make
make install
```

```bash
# FFmpeg support
git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg
cd ffmpeg
git checkout release/6.0
./configure --disable-all --disable-autodetect --disable-podpages --disable-asm --enable-avcodec --enable-avformat --enable-decoders --enable-encoders --enable-demuxers --enable-parsers --enable-protocol='file' --enable-swscale --enable-zlib
make
sudo make install
```
### (RT)OS support
Works with any OS like pthred, Windows, FreeRTOS, etc. It has build in support for FreeRTOS. 

## Test
This project is configured for [VSCode](https://code.visualstudio.com) and is tested on: 
- Ubuntu Linux 
- Windows WSL (Ubuntu Linux)

It requires a working version of GCC, GDB and make in your path.

To allow debugging inside VSCode you will also require a GDB [extension](https://marketplace.visualstudio.com/items?itemName=webfreak.debug) or other suitable debugger. All the requirements, build and debug settings have been pre-configured in the [.workspace](simulator.code-workspace) file.

The project can use **SDL** but it can be easily relaced by any other built-in LVGL dirvers.

## Integration with LVGL Pro

This project supports integration with LVGL Pro projects for UI development.

### Setup

1. Configure CMake with your LVGL Pro project folder:

```bash
cmake -B build -DLVGL_PRO_PROJECT_DIR=<path-to-lvgl-pro-project>
```

Build your project:

```bash
cmake --build build
```

### Usage in Code

In your main.c, include the UI header from your LVGL Pro project and replace the default demo with your screen.

```c
#include "ui.h"

int main(void) {

    /*Initialization code for LVGL*/
    
    /* Initialize the LVGL Pro UI */
    ui_init("<path-to-lvgl-pro-project>");
    
    /* ... rest of your application ...*/
}
```

### 待机眨眼

待机每 2.8–5.2 秒眨眼一次（闭合 65ms、停留 55ms、睁开 85ms）。眼皮通过 LVGL 绘制，仅覆盖正面贴图的眼睛区域，不更换或缩放整张角色图。点击进入轻跳时立即睁眼并暂停眨眼，落地后重新计时；走路、饥饿、睡眠不显示此眼皮。删除宠物时清理眼皮动画与计时器。

可选回归检查（使用可控时钟与真实 LVGL 渲染，无需 X server）：

```sh
cmake --build build --target blink_test
./bin/blink_test
```

演示见仓库 `docs/acceptance/blink/blink.gif`。演示为真实 LVGL 帧缓冲，按 33ms 步进；首次眨眼在录制中提前至 1.8 秒，之后使用正常随机间隔，3.3 秒触发轻跳。

### 待机轻拍翅膀

待机每 6.5–11 秒轻拍两下翅膀，每次抬起 180ms、落下 230ms，两次之间停 90ms。左侧最多转动 18°，右侧 16.2°；只转动翅膀，脚底与身体布局保持原位。眨眼可以同时发生。点击身体或旋转后的翅膀仍触发原有轻跳；切换状态立即收回并隐藏独立翅膀，回到待机后重新计时。

使用现有 BGRA 角色的运行时部件拆分与 LVGL 旋转，不修改原始 PNG；身体与翅膀缓冲新增约 157KiB 静态 RAM，真机内存和帧率仍需验证。回归检查补充两次拍动、全部阶段被轻跳打断、旋转翅膀点击、其他状态隐藏及删除清理。

演示：`docs/acceptance/wings/wings.gif`，来自真实 LVGL 帧缓冲；首次拍翅提前到 1.5 秒，首次眨眼在 1.8 秒，3.3 秒触发轻跳，之后使用正常间隔。
