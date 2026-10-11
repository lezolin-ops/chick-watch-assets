# 真机移植通关记录（FIRMWARE）

> 微雪 ESP32-S3-Touch-AMOLED-2.06 · ESP-IDF 工程 · **本文件是踩坑沉淀，任何工具/人接手先读这里，避免重复犯错**

## 0. 一句话状态

`firmware/` 基于官方 `02_lvgl_demo_v9` 例程改造：BSP 初始化显示+触摸+LVGL **v9.6.0**（2026-10-11 由 9.5.0 升版，与模拟器统一，见 §3.10）→ `pet_create(lv_screen_active(), 410, 502)` 直接跑模拟器同款宠物逻辑。素材 156 个 C 数组文件已全部拷入 `main/`。**固件已烧录真机（2026-10-11，8.66MB → factory 10M 分区），待人工验收画面。**

## 1. 环境基线（已验证可用的组合）

| 项 | 值 |
|-|-|
| ESP-IDF | **v5.4.2**（`~/esp/esp-idf-5.4/`，必须 ≥5.4，见 §3.3） |
| 工具链 | 已装 `~/.espressif/tools`（xtensa-esp-elf 14.2 / riscv / openocd / gdb） |
| cmake/ninja | 系统 PATH 没有 → 用项目自带：`export PATH=/Users/Zhuanz1/Documents/trae_projects/mini/tools/venv/bin:$PATH` |
| 开发板 USB | `/dev/cu.usbmodem1101` |
| 官方例程 | `~/esp/waveshare-2.06/examples/esp-idf/02_lvgl_demo_v9`（BSP 组件在组件管理器拉取） |

## 2. 完整编译命令（直接复制可用）

```bash
export PATH=/Users/Zhuanz1/Documents/trae_projects/mini/tools/venv/bin:$PATH
source ~/esp/esp-idf-5.4/export.sh
cd /Users/Zhuanz1/Documents/trae_projects/mini/firmware
idf.py build            # 首次或跨版本先: rm -rf build managed_components && idf.py set-target esp32s3
idf.py -p /dev/cu.usbmodem1101 flash monitor   # 烧录+串口日志
```

## 3. 踩坑清单（按出现顺序，全部已解决）

### 3.1 工具链安装：GitHub 直连必超时
- `./install.sh esp32s3` 从 GitHub releases 下载 → **Operation timed out**（走了死代理或直连被拦）
- ✅ 解法：`IDF_GITHUB_ASSETS=dl.espressif.com/github_assets ./install.sh esp32s3`（Espressif 官方国内镜像，秒下）

### 3.2 git 代理：URL 级代理指向死端口
- 本机 git 全局配置 `http.https://github.com/.proxy=http://127.0.0.1:7897`（VPN 工具代理）
- **代理没开时**：所有 git 网络操作连 127.0.0.1:7897 失败（clone/submodule 全挂）
- ✅ 解法 A（用户开 VPN 后）：直接 `git submodule update --init --recursive`
- ✅ 解法 B（不想开 VPN 且直连可用时）：`git -c http.https://github.com/.proxy= submodule update ...`（注意是 URL 级 key，`-c http.proxy=` 无效）

### 3.3 IDF 版本：必须 ≥5.4
- v5.3.2 编译官方例程报：`espressif/usb (^1.4.1) depends on idf (>=5.4)` + BSP 的 `sdmmc_slot_config_t` 字段不兼容
- ✅ 解法：升级 `git clone --depth 1 -b v5.4.2 --recursive` 到 `~/esp/esp-idf-5.4`，重新 install + 编译。**官方例程、BSP v2、usb 组件、sdkconfig 全是按 5.4+ 写的**

### 3.4 依赖精简：去掉音频/USB 组件
- 官方 `idf_component.yml` 带 `espressif/usb`（USB 功能栈）+ `espressif/esp_codec_dev`（ES8311 音频 codec）——**电子宠物用不到**，且 usb 组件是版本冲突源头
- ✅ 解法：`main/idf_component.yml` 只留 `waveshare/esp32_s3_touch_amoled_2_06 ^2.0.0` + `lvgl/lvgl 9.5.0`
- 注意：BSP 传递依赖会**自动拉回 esp_codec_dev**（不影响编译）。**后面要发声时**：加回 esp_codec_dev（IDF 已升 5.4 无冲突），板载 ES8311 可用

### 3.5 lvgl 组件头文件误报：RT-Thread 支持文件
- 报错：`Missing "lvgl/lvgl.h" ... env_support/rt-thread/squareline/ui/lvgl/lvgl.h`
- 原因：LVGL 自带的 RT-Thread 支持文件路径触发 ESP-IDF 头文件校验误报（用不到 RT-Thread）
- ✅ 解法：`rm -rf firmware/managed_components/lvgl__lvgl/env_support/rt-thread`
- ⚠️ **每次 `rm -rf build managed_components` 重新 resolve 后该目录会回来，需再删一次**

### 3.6 lvgl include 前缀：`lvgl/lvgl.h` → `lvgl.h`
- 模拟器 include 路径带 `lvgl/` 前缀可用；真机 lvgl 组件 include 根是源码根，`#include "lvgl/lvgl.h"` 编译不过
- ✅ 解法（firmware/main 下）：`grep -rl '#include "lvgl/lvgl.h"' . | xargs sed -i '' 's|#include "lvgl/lvgl.h"|#include "lvgl.h"|g'`（**129 个文件**）
- ⚠️ **重新拷贝素材后必须重做此替换**（cp 会用模拟器源文件覆盖）

### 3.7 素材文件必须拷全（156 个）
- pet.c 直接 include 26 个，素材间**嵌套 include**（pet_daily_sprites.c → pet_daily_right_0.c 等帧文件）
- ✅ 解法：`cp lvgl-simulator/src/pet_*.c pet_*.h bg_*.c firmware/main/`（156 个，全部是被 include 的素材，不会重复编译；CMakeLists SRCS 只有 `main.c pet.c`）
- 报 `#include "pet_daily_right_0.c"` 找不到就是没拷全

### 3.8 LVGL API 版本差异：9.6 → 9.5（2026-10-11 已升 9.6.0，转换待还原，见 §3.10）
- 模拟器 lvgl **v9.6.0-master**（`lv_version.h` 写 10.0.0-dev 只是 master 版本号预告，API 是 9.6 系），真机当时 9.5.0
  | 9.6 写法 | 9.5 等价 |
  |-|-|
  | `lv_obj_set_clickable(o, true/false)` | `lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE)` / `lv_obj_clear_flag(...)` |
  | `lv_obj_set_scrollable(o, bool)` | `lv_obj_add_flag/clear_flag(o, LV_OBJ_FLAG_SCROLLABLE)` |
  | `lv_obj_set_hidden(o, bool)` | `lv_obj_add_flag/clear_flag(o, LV_OBJ_FLAG_HIDDEN)` |
  | `lv_obj_set_hidden(o, 表达式)` | `lv_obj_set_flag(o, LV_OBJ_FLAG_HIDDEN, 表达式)` |
- ✅ 已批量转换（pet.c + pet_scene.h），验证：`grep -rn "lv_obj_set_hidden\|lv_obj_set_clickable\|lv_obj_set_scrollable" main/` 应为 0

### 3.9 出厂固件备份（烧录前必做）
```bash
python3 ~/esp/esp-idf-5.4/components/esptool_py/esptool/esptool.py \
  --chip esp32s3 -p /dev/cu.usbmodem1101 -b 460800 \
  read_flash 0x0 0x1000000 firmware/backup/factory_flash_16MB.bin
```
- ✅ 已完成：`firmware/backup/factory_flash_16MB.bin`（16MB 全量，恢复出厂用它写回）
- 防砖原理：ROM bootloader + USB-Serial-JTAG 在芯片内部，刷坏也能按住 BOOT 进下载模式重刷；正常 `idf.py flash` 不碰 efuse，不会真砖

### 3.10 LVGL 升版 9.5.0 → 9.6.0（与模拟器统一版本，2026-10-11）
- 背景：模拟器 lvgl 实际是 v9.6.0-master（FIRMWARE §3.8 的 9.6 vs 9.5 差异只有 3 个便捷函数）；统一版本消除每次拷素材的手动转换
- **注意**：ESP 组件注册表有 9.6.0（2026-09-16 上传，`components.espressif.com/api/components/lvgl/lvgl` API 为准；网页缓存快照显示 "9.5.0 Latest" 是误导）
- ✅ 改动：`main/idf_component.yml` → `lvgl/lvgl: "^9.6.0"`，重新 resolve + 全量重编
- ⚠️ 9.6.0 中 `lv_obj_add_flag/clear_flag/set_flag` 已标 **deprecated**（建议用 `lv_obj_set_hidden/clickable/scrollable` 等便捷 setter）→ §3.8 的 29 处转换会产生编译警告，**应还原为便捷写法**（待做，见 §5）
- 真机 46 个 API 全部在 9.6.0 存在（含宏映射），编译通过

### 3.11 PET_EXT_RAM_BSS 条件宏失效：大像素缓冲落内部 DRAM，链接溢出 152KB
- 现象：链接报 `region 'dram0_0_seg' overflowed by 152096 bytes`，map 里 `pet.c.obj` 在 `.dram0.bss` 有 2×144KB（`tuft_pixels` / `walk_expression_pixels`，均为 192*192*4）
- 原因：`pet.c`/`pet_tuft.h` 用 `#if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY)` 决定 `PET_EXT_RAM_BSS`，但 **main 组件编译单元里这些 CONFIG 宏不可见**（compile_commands.json 无 `-D CONFIG_*`、无 `-include sdkconfig.h`）→ 条件恒假 → 宏退化为空 → 缓冲落进内部 DRAM
- ✅ 解法：改为保护式定义，4 个文件统一（pet.c / pet_tuft.h / pet_walk_expression.h / pet_front_wing.h）：
  ```c
  #include "esp_attr.h"
  #ifndef PET_EXT_RAM_BSS
  #define PET_EXT_RAM_BSS EXT_RAM_BSS_ATTR
  #endif
  ```
- 验证：链接通过，`.ext_ram.bss` 段接管缓冲，DRAM 溢出消失

### 3.12 分区扩容：factory 8M → 10M（固件镜像超分区 675KB）
- 现象：`pet_watch.bin` 8.66MB（素材 ARGB8888 占 `.flash.rodata` 8.08MB + 代码 0.4MB）> factory 8M
- ✅ 改动：`partitions.csv` → `factory 10M, storage 5M`（storage 暂未使用；**素材压缩后应回退 8M**，见 §5）
- 当前余量：0x157400 ≈ 1.35MB（13%）

## 4. 当前产物

| 产物 | 位置 | 状态 |
|-|-|-|
| 固件工程 | `firmware/`（CMakeLists=pet_watch，main.c=nvs+bsp_display_start+pet_create） | ✅ 编译通过（lvgl 9.6.0） |
| 出厂备份 | `firmware/backup/factory_flash_16MB.bin` | ✅ 16MB |
| 固件镜像 | `firmware/build/pet_watch.bin` | ✅ 8.66MB，**已烧录真机 2026-10-11** |

## 5. 下一步

1. **真机验收**：屏幕显示小鸡 + 触摸开心（模拟器已验收，真机待人工确认；`idf.py -p /dev/cu.usbmodem1101 monitor` 看 boot 日志）
2. **素材压缩（排期中）**：156 个素材 ARGB8888(4B/px) → RGB565A8(3B/px) 预计 ~6.6MB，可回退 factory 8M 分区；源 PNG 在 `assets/chick_character_design_pack/`，生成脚本 `lvgl-simulator/tools/generate_lvgl_argb8888.py` 改格式后重新生成，模拟器+真机双端同步验证
3. **API 还原（顺手做）**：§3.8 的 29 处 flag 转换还原为 `lv_obj_set_hidden/clickable/scrollable`（9.6.0 便捷 setter），消除 deprecated 警告、与模拟器源码逐字一致
4. 里程碑二（发声）：IDF 已 5.4，加回 `espressif/esp_codec_dev` + I2S 驱动即可

## 6. 官方烧录说明通读要点（2026-10-11 补充，烧录前必看）

> 来源：微雪 wiki「ESP32-S3-Touch-AMOLED-2.06」整页（含 FAQ）+「Flash Firmware Flashing and Erasing」通用教程。已通读核验。

### 6.1 官方自制固件（确认存在）

- 官方 Demo 包 `..\ESP32-S3-Touch-AMOLED-2.06-Demo\Firmware` 提供 **测试固件 bin**（验证板载功能是否正常）+ **FactoryFirmWare**（esp-brookesia 成品级固件：触摸/显示/录音/播放/时钟/陀螺仪/音视频/PMU 控制，持续更新）
- 官方烧录工具 = `Flash_download_tool`（Windows）；本机恢复出厂直接用已备份 `firmware/backup/factory_flash_16MB.bin`（esptool 写回 0x0 起 16MB）
- 官方明确：**ESP-IDF 例程启用了双缓冲 + 防撕裂优化，比 Arduino 流畅**（动态 20~30 fps）→ 本项目走 ESP-IDF 路线正确，帧率预期可参考

### 6.2 本板按键与进下载模式（与通用教程不同，重点！）

- **本板没有 RESET 键**，只有 BOOT（GPIO0）+ PWR（GPIO1）两个侧键 → 通用教程的「BOOT+RESET」组合**不适用本板**
- 进下载模式（程序崩溃 / 刷死 / USB GPIO 被占用）：**完全断电 → 按住 BOOT → 重新上电（接 Type-C）** → 强下载模式
- 强下载模式烧录完**不会自动退出**，必须断电重启
- 开机/关机：按住 **PWR ≥6 秒关机**，再**点按 PWR 开机**；插 USB 供电可直接上电
- 烧录成功后串口显示 "waiting for download..."：**再上电一次**即可正常启动

### 6.3 烧录失败的典型原因（FAQ 原文口径）

| 现象 | 解法 |
|---|---|
| 串口被占用 → 烧录失败 | 关闭串口监视器（monitor）再烧 |
| 程序崩溃 → 烧录失败 | 断电 → 按住 BOOT 上电 → 强下载模式 → 烧完断电重启 |
| 模块反复重启 | 同上，程序崩溃所致，重刷固件解决 |
| 编译失败 | 删 build + managed_components + dependencies.lock 后重编（重 resolve 后需再删 RT-Thread 目录，见 §3.5） |

### 6.4 串口日志（验证固件用）

- Type-C 口 = ESP32-S3 USB 直出；ESP-IDF 用 `ESP_LOGD/ESP_LOGI` 打印（`Serial`/`printf` 是 Arduino 口径，需开 USB CDC 才有效）
- 验证链路：`idf.py monitor` 看 boot 日志 → 屏幕小鸡画面 + 触摸响应（人工确认）

### 6.5 硬件事实（后续开发参考）

- 电量百分比波动大属正常（AXP2101 按电压非线性估算），判断电量以电压为准
- 发热：WiFi/BT 开启最高约 46℃，不开约 36℃；AMOLED 40–60℃ 不影响显示/触摸 → 宠物省电需走 ESP32-S3 低功耗模式
- 电池建议 4×27×28 400mAh：全亮约 1h、关背光 3–4h、全低功耗约 6h
- 按键可编程：BOOT 低电平按下（单击/双击/多击/长按）；PWR 高电平按下（长按 >6s 会关机）
- 中文显示：需 Unicode-8 编码字符串 + 字库 + LVGL/GFX 支持
- 亮度控制：QSPI 写 0x51 寄存器 0x00~0xFF（最大 600nit）

### 6.6 问题支持渠道（按问题类型分流，官方/社区）

| 问题类型 | 渠道 | 说明 |
|---|---|---|
| 板子 / 例程 / 烧录 / 硬件 | 微雪官方 wiki 产品页（含 FAQ）+ GitHub Demo 仓库 | 产品页是首查；GitHub 可订阅仓库并提 issue，微雪工程师会评估需求（官方 FAQ 明确支持） |
| 框架层（ESP-IDF / LVGL / BSP） | 乐鑫官方文档 + ESP32 Forum；LVGL GitHub issues | 编译/API/驱动问题先查 ESP-IDF 文档与论坛，比问微雪更快 |
| 同类现成方案 / 参考实现 | 官方 wiki「Project Resources」第三方项目 + Reddit r/esp32 + GitHub 搜索 | 2.06 已有开源项目：VolosR/waveshareControl、Tsixom0/Waveshare-esp32-s3-2.06-amoled-AI-Assistant、fbiego/chronos-navio |
| UI 设计（SquareLine） | SquareLine Studio 官方教程 | 官方 wiki FAQ 提供入口 |
| 商务 / 批量定制 | 微雪销售团队 | 官方不代改代码（DIY 定位），批量定制找商务 |
