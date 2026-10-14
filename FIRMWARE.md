# 真机移植通关记录（FIRMWARE）

## 联网校时（2026-10-12）

新增取得IP后一次后台SNTP校时、RTC写回及网络页Wi-Fi开关。联网工作任务曾因新增局部完整AP状态快照叠加导致6144字节栈溢出；现已改为只读必要标志并使用8192字节栈，校时任务6144字节。最终压缩镜像3983568字节，刷写哈希通过；55秒启动日志确认取得IP、SNTP成功和RTC写回ESP_OK，没有重启循环。详见 `firmware/TIME_SYNC.md`，手动开关与完整关机场景待验收。

## Wi-Fi 设置增量（2026-10-11）

新增 BOOT 设置入口、手表扫描并直接输入 Wi-Fi 密码、成功保存与重启自动重连；支持全屏 QWERTY、大小写/数字/额外符号、按键放大、删除、密码显隐。亮度持久化暂限10–50%；OTA 仅保留未接入入口。Flash容量配置已与实测统一为32MB，现有分区布局不变。

固件镜像约3.79MiB（加入联网组件）；ESP-IDF编译与真实LVGL模拟交互/渲染验证通过，实际联网待验收。实现依据、限制、步骤及测试见 `WIFI_SETTINGS.md`。刷写前备份路径 `backup/pre_wifi_flash_20261011.bin`（备份及刷写完成情况以本次记录后续核验为准）。

2026-10-12核验：前16MiB备份已完成；首次Wi-Fi镜像刷写哈希通过，但启动在显示驱动的旋转缓冲分配失败后发生InstrFetchProhibited并反复重启。修复为固定竖屏关闭 `.sw_rotate` 的多余内部缓冲（`patches/fixed_portrait_memory.patch`），并开启 `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP`、关闭Wi-Fi IRAM/RX IRAM优化。修复版3972592字节已编译；再次刷写时USB串口消失，修复尚未进入设备，必须重新连接后刷写并验证启动，不得标记真机验收通过。

同日后续核验：设备重新连接到 `/dev/cu.usbmodem101` 后修复版已刷入且哈希通过，15秒启动日志确认32MB、显示、Wi-Fi站点模式初始化正常，无panic/重启循环。用户随后要求亮度开放100%，保留默认50%；实现、交互检查和压缩构建通过，最终刷写及启动检查见PRODUCT_UPDATES的PU-009。

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

### 3.8 LVGL API 版本差异：9.6 → 9.5（2026-10-11 已升 9.6.0，转换已还原）
- 模拟器 lvgl **v9.6.0-master**（`lv_version.h` 写 10.0.0-dev 只是 master 版本号预告，API 是 9.6 系），真机当时 9.5.0
  | 9.6 写法 | 9.5 等价 |
  |-|-|
  | `lv_obj_set_clickable(o, true/false)` | `lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE)` / `lv_obj_clear_flag(...)` |
  | `lv_obj_set_scrollable(o, bool)` | `lv_obj_add_flag/clear_flag(o, LV_OBJ_FLAG_SCROLLABLE)` |
  | `lv_obj_set_hidden(o, bool)` | `lv_obj_add_flag/clear_flag(o, LV_OBJ_FLAG_HIDDEN)` |
  | `lv_obj_set_hidden(o, 表达式)` | `lv_obj_set_flag(o, LV_OBJ_FLAG_HIDDEN, 表达式)` |
- ✅ 真机侧已还原为 9.6 便捷写法（2026-10-11，29 处 flag → `lv_obj_set_hidden/clickable/scrollable`），deprecated 警告清零、与模拟器源码一致；验证：`grep -rn "lv_obj_add_flag\|lv_obj_clear_flag\|lv_obj_set_flag" main/` 应为 0

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
- ⚠️ 9.6.0 中 `lv_obj_add_flag/clear_flag/set_flag` 已标 **deprecated**（建议用 `lv_obj_set_hidden/clickable/scrollable` 等便捷 setter）→ §3.8 的 29 处转换已**还原为便捷写法**（2026-10-11，见 §5）
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

### 3.13 真机固定灰色横条：QSPI 屏误走 RGB 刷新路径（2026-10-11，已验收）

- **现象**：官方出厂固件正常；本项目固件出现肉眼可见的灰白横条，排除仅由手机拍摄造成的条纹。
- **根因链路**：本板初始化的是 QSPI 面板，但 BSP 的 `bsp_display_lcd_init()` 调用了 `lvgl_port_add_disp_rgb()`。该入口将显示类型设为 RGB；局部刷新后立即 `lv_disp_flush_ready()`，而 SPI DMA 仍可能读取行缓冲。LVGL 提前复用缓冲，导致显示数据被覆盖。
- **本次唯一功能修复**：改为 `lvgl_port_add_disp(&disp_cfg)`，由该 SPI 接入路径注册 `.on_color_trans_done` 回调，传输完成后才允许复用绘图缓冲。删除不适用的 RGB 专用配置，并加启动日志 `QSPI display: SPI completion callback, 16400-pixel DMA buffer`。
- **保留已有设置**：40 行内部 DMA 缓冲、32,800 字节最大传输、队列深度 4、亮度 50%、33ms 刷新周期；未改小鸡素材、动画、分区表或 NVS。
- **配置核验**：`sdkconfig.defaults` 中写了 `CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH=y`，但它依赖 RGB 抗撕裂选项，实际 `sdkconfig`/生成头文件没有启用。因此此前“全刷新已消除灰线”的注释不是实测结论。本次不是靠启用全刷新修好的。
- **构建与烧录**：ESP-IDF 5.4.2 增量构建通过；新镜像 `0x8a8400` 字节，10M 应用分区仍有约 13% 余量。通过 `idf.py -p /dev/cu.usbmodem1101 -b 460800 app-flash` 仅更新 `0x10000` 应用分区，保留 bootloader、分区表及 NVS。
- **验收**：用户刷入后明确回复“正常了”，确认灰线已消失。触摸、长时间稳定性、功耗不因此自动视为已验收。
- **回退备份**：旧项目固件 `firmware/backup/pet_watch_before_qspi_fix_20261011.bin`；原出厂备份 `firmware/backup/factory_flash_16MB.bin` 未覆盖。
- **补丁持久化**：managed component 默认不入仓库，重新下载会覆盖本地修改。已保存 `firmware/patches/qspi_flush_completion.patch` 与使用说明；重新 resolve 后必须检查并重施该补丁，保留/核对之前的 DMA 与传输大小调整。

### 3.14 资源无损压缩与后续开发流程（2026-10-11，已构建/主机验证，待压缩版真机验收）

- **目标**：缩小固件，不删除散步等动画、不降低颜色/透明度精度、不恢复分区；保留后续素材和代码开发能力。
- **方式**：原始 PNG 和 `firmware/main/` 原始 C 图片不改。`idf.py build` 自动生成 `build/generated/pet_packed_assets.c`；使用 LVGL 的标准 LZ4 压缩头及原有 ARGB8888 格式。打包时逐资源解压与原始字节比较，并输出 SHA-256/CRC32 清单；重复负载共享存储，未引用资源仍由链接器裁除。
- **运行兼容**：眨眼、呆毛、翅膀和植物代码仍读取原来的未压缩 32 位像素。新增资源访问层负责解压；姿态使用六槽 LRU、稳定描述符地址和最近两姿态保护。解压缓存上限 3MiB，ESP32 显式分配在 PSRAM，不挤占显示 DMA 的内部 SRAM。
- **测量**：129 个输入资源合计 11,907,808 字节 → 压缩负载 3,714,643 字节（含未最终链接的备用素材，不能把该总数等同于固件大小）。最终压缩固件约 3.23MiB，原运行固件约 8.66MiB；同源码关闭压缩的构建约 8.81MiB，包含同步开发中的时钟代码和当前框架配置。10MiB 应用分区保留，压缩版余量约 68%。
- **验证**：129 个资源的原生解码 CRC32 全部匹配；1,200 次姿态访问/驱逐/重载验证通过。对实际固件 `pet.c` 分别按原始/压缩模式执行完整回归，两组共对应 936 张画面，逐文件字节比较 0 差异。散步、回中、转头、眨眼、扇翅、嘴部及睡眠暂停检查均通过；回归中解压缓存峰值 2,693,136 字节（约 2.57MiB），6 个活跃姿态。此数不含程序其他 RAM，亦不是实际真机 FPS 测量。
- **开发入口**：修改/同步原始素材后正常构建即可；生成物不手改。`idf.py -D PET_PACK_RESOURCES=OFF build` 可做原始编码对照，`ON` 恢复默认。两种 ESP-IDF 构建均已验证；原始编码对照镜像另存 `firmware/backup/pet_watch_raw_resource_reference_20261011.bin`（不宣称与当前设备烧录版本完全相同）。
- **安全/持久化**：保留 QSPI 修复和已有 DMA/亮度/刷新设置；修正 `sdkconfig.defaults` 的行内注释格式，确保新配置仍读到 40 行缓冲及 33ms 刷新，而非悄悄回退默认值。此资源优化阶段没有执行设备烧录；压缩版画面、触摸、FPS 与实际 RAM 仍需真机验收。
- **说明与一键检查**：`firmware/RESOURCE_OPTIMIZATION.md`；`python3 firmware/tools/check_resources.py` 自动完成打包、缓存压力测试及原始/压缩全帧对照。未来多角色/换场景需按文档管理资源寿命，不能无上限保留全部解压图。

### 3.15 横排钟表与自动黑屏（2026-10-11）

- 已合并资源压缩、RTC 钟表、PMU 电量/充电显示、电源键短按切换与横排大字。
- 钟表页 10% 亮度，每分钟整组 ±4px 移位，进入后 60 秒全黑并将 AMOLED 亮度归零；充电不延长显示。黑屏仅电源键唤醒回小鸡，保留原有小鸡页 15 秒无操作转钟表和 PMU 长按行为。
- 字号、交互与验收步骤见 `firmware/WATCH_DISPLAY.md`。黑屏并非 CPU 深睡眠，PMU 轮询仍工作。
- 最新固件 3,432,208 字节（约 3.27MiB），应用刷写及哈希校验通过，自动重启；用户反馈压缩后帧率上升，未进行同条件量化 A/B 测量。新版短按、字号及 60 秒黑屏尚待用户验收。

## 4. 当前产物

| 产物 | 位置 | 状态 |
|-|-|-|
| 固件工程 | `firmware/`（CMakeLists=pet_watch，main.c=nvs+bsp_display_start+pet_create） | ✅ 编译通过（lvgl 9.6.0） |
| 出厂备份 | `firmware/backup/factory_flash_16MB.bin` | ✅ 16MB |
| 固件镜像 | `firmware/build/pet_watch.bin` | ✅ 约3.27MiB压缩版含钟表/自动黑屏，已刷入并校验；最新交互待真机验收 |

## 5. 下一步

1. **真机验收**：屏幕显示小鸡 + 触摸开心（模拟器已验收，真机待人工确认；`idf.py -p /dev/cu.usbmodem1101 monitor` 看 boot 日志）
2. **素材压缩已实现（见 §3.14）**：采用无损 LZ4，仍保持 ARGB8888 和完整动画；压缩版约3.23MiB，待真机验证。先保留10MiB分区，不因能够装进8MiB就立即改分区。
3. ~~API 还原~~：**✅ 已完成（2026-10-11）**，29 处 flag → 便捷 setter（`lv_obj_set_hidden/clickable/scrollable`），编译无警告，已重新烧录
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

### 6.7 真机验收操作流程（USB 供电 · 未装电池场景，2026-10-11）

**烧录完成后按此走：**
1. **断电重启 = 拔掉 USB → 重新插上**（USB 直连供电场景下拔插即断电重启；PWR 键关机/开机是电池场景逻辑，USB 插上即上电）
2. 若串口显示 "waiting for download..."：**拔插一次 USB** 即可正常启动（不是烧坏，§6.2）
3. **看屏幕**：应显示小鸡画面（模拟器已验收的默认态）；黑屏/花屏进异常分支
4. **看日志**：`idf.py -p /dev/cu.usbmodem1101 monitor`，应有正常 boot 日志（ESP_LOGI），**无反复重启 / 无 panic**
5. **触摸验证**：点按屏幕看宠物反应（开心等交互，模拟器已验收）
6. **收尾**：`Ctrl+]` 退出 monitor —— monitor 占用串口，不退出会影响下次烧录（§6.3）

**异常分支：**
- 反复重启 / 无输出 → 程序崩溃：拔 USB → **按住 BOOT 插 USB**（强下载模式）→ 重烧 → 再拔插 USB 重启
- 屏幕黑但日志正常 → 先查 touch/display 初始化是否报错；再查背光（BSP 默认开启）

**装电池后补充**：重启 = PWR ≥6s 关机 → 点按 PWR 开机；可测电池电压/续航（AXP2101，§6.5）

## 7. 息屏时钟原型（2026-10-11）

- 新模块 `main/watch_clock.c`：纯黑背景、24 小时制大时间、数字日期与英文星期；不开秒数或步数占位；下方显示真实电量与充电状态。
- 开机直接显示时钟方便预览；触摸返回宠物，闲置 15 秒重新进入时钟。时钟亮度 10%，宠物亮度维持 50%。暂停宠物的三个动画定时器，唤醒后继续；尚未接入 ESP32 深睡/自动轻睡，功耗未经真机测试。
- 优先读取 PCF85063（I2C 0x51）；RTC 停振、时间无效时用编译时间初始化，无法读取时使用系统时钟加编译时间兜底。北京时间；尚无联网校时/手动校时入口，旧 RTC 时间也可能需要校准。
- 时钟每秒检查时间，只在分钟变化时更新标签并轻微移动位置。
- ESP-IDF 5.4.2 构建通过，未压缩预览镜像 `0x8ceb70` 字节，10M 应用分区约 12% 余量。预览 `assets/previews/watch-clock-v1.png` 使用模拟器 LVGL 绘制；不是屏幕实拍。
- 素材压缩由另一个聊天并行开发。钟表验收使用独立构建目录：（从项目根目录执行）`idf.py -C firmware -B build-clock-preview -D PET_PACK_RESOURCES=OFF build`；预览镜像位于 `build-clock-preview/pet_watch.bin`。此构建目录隔离产物，但两个聊天仍共享源文件。
- 本次电脑未发现手表串口，未烧录；连接后用 `idf.py -C firmware -B build-clock-preview -D PET_PACK_RESOURCES=OFF -p PORT app-flash` 仅更新应用分区，遵循 §3.13 的已有 QSPI 修复。

### 7.1 电池显示

读取 AXP2101（I2C 0x34）：状态寄存器 0x00/0x01 与百分比寄存器 0xA4，参考官方 01_AXP2101 的 XPowersLib。电池图标随电量变化；充电显示闪电，充满显示 FULL，无电池且检测到 USB 时显示 USB，无电池无 USB 显示 NO BATTERY，读取失败/百分比超范围显示 --%。不修改充电配置或电源输出。时钟显示时每 5 秒读取，文字有变化才重绘；进入时钟立即刷新。

构建通过；7 个模拟状态检查通过（充电、充满、USB 无电池、无电池无 USB、放电、无效电量、读取失败）。`assets/previews/watch-clock-battery-v1.png` 为 78% 充电状态的 LVGL 布局示例，非真机电量；尚未连接手表验收。
