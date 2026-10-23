# 电子宠物手表（pet-watch）

> 载体：微雪 ESP32-S3-Touch-AMOLED-2.06（410×502 AMOLED 触控屏）
> 框架：LVGL v9（ESP-IDF 真机路线 + LVGL PC 模拟器先行开发）
> 开发模式：AI Agent 开发（用户为产品经理，负责产品文档与验收，不提交代码）
> 状态：**真机阶段——设备运行 v1.0.0（睡眠 v1 + 电源优化，2026-10-15 刷入）；当前工作在情绪动画素材与模拟器预览，尚未进固件**（截至 2026-10-18）

硬件规格见 [HARDWARE.md](HARDWARE.md)，它是模拟器分辨率、素材尺寸和真机移植的统一基线。

官方资料、本地代码差异、续航及外设诊断见 [微雪2.06开发与诊断指引](docs/reference/waveshare-2.06-guide.md)，附八个官方产品章节的阅读范围与来源，未核验图纸及手册单独标注。

续航优化、独立开关、应用电源需求接口及短时A/B方法见 [电源管理与调优](firmware/POWER_MANAGEMENT.md)；后台电池记录与导出见 [电池记录](firmware/BATTERY_LOG.md)，睡眠三阶段见 [睡眠 v1](firmware/SLEEP_V1.md)。

界面开发统一遵循 [手表UI / UE设计规范](docs/design/watch-ui-ux-guidelines.md)，涵盖圆角安全区、视觉、键盘、按键导航、亮度息屏、异常流程和验收。

新增功能与产品决策见独立的 [产品更新说明](PRODUCT_UPDATES.md)。开发助手按 [项目协作规则](AGENTS.md) 在每次任务结束前自动维护，并区分设计、实现和验收状态。

当前设备装的是哪一版、历史版本验收到哪一步，以 [固件台账](firmware/MANIFEST.md) 为准（命名规范、SHA256、验收状态）。情绪动画的序列结构与生产规范见 [动画序列说明书](assets/chick_character_design_pack/动画序列说明书.md)、[动画生产经验](assets/chick_character_design_pack/happy_keyframes/动画生产经验.md)；单动作预览在模拟器工作台，见 [PREVIEW_WORKSPACE.md](lvgl-simulator/PREVIEW_WORKSPACE.md)。

## 一、项目目标

在微雪 2.06 手表开发板上做一个"电子宠物"——固件内置应用，关键帧贴图 + 程序补间/程序化动画，可实时响应触摸与六轴感应，具备生命感与互动感。

当前覆盖：桌面宠物（日常行为、睡眠、点击开心）、时间日期/时钟、设置、Wi-Fi 配网与联网校时、电源管理与屏幕休眠、后台电池记录、空闲外设诊断。六轴感应（摇一摇）尚未接入。

## 二、目录结构

| 目录/文件 | 说明 | 状态 |
|-|-|-|
| `README.md` | 项目总览（本文档） | ✅ |
| `HARDWARE.md` | 用户确认的手表硬件配置与项目约束 | ✅ |
| `AGENTS.md` | 项目协作规则（文档维护、UI 规范、发布流程） | ✅ |
| `PRODUCT_UPDATES.md` | 产品更新说明与决策记录（PU 编号递增） | ✅ |
| `电子宠物动画系统技术实现清单.md` | 技术清单（状态机/动画分级/精灵图/执行顺序/验证方式） | ✅ |
| `lvgl-simulator/` | LVGL PC 模拟器工程（编译产物 `bin/main`） | ✅ 已跑通 |
| `lvgl-simulator/src/pet.c` `pet.h` | 宠物核心模块（与固件各一份、逻辑同源，改动需手动同步；预览专用代码只在此侧） | ✅ |
| `lvgl-simulator/PREVIEW_WORKSPACE.md` | 动作预览工作台：`--happy-demo` / `--angry-demo` / `--sleep-demo`，冻结日常只循环单动作 | ✅ 预览待肉眼验收 |
| `LOTTIE.md` | Lottie 动画路线专档：ThorVG 兼容字段规范、渲染顺序、调试经验 | ✅ 最小验证通过，当前非主线 |
| `firmware/` | 真机 ESP-IDF 工程（含 129 项压缩素材） | ✅ v1.0.0 已烧录真机并运行 |
| `firmware/MANIFEST.md` | 固件台账：命名规范、SHA256、各版本验收状态（权威清单） | ✅ |
| `firmware/*.md` | 电源/睡眠/电池记录/Wi-Fi/校时/显示/圆角安全区/发布流程技术说明 | ✅ |
| `FIRMWARE.md` | 真机移植通关记录：环境基线/编译命令/全部踩坑与解法（接手必读） | ✅ |
| `assets/` | 宠物形象素材：参考图、角色设定包、转面/表情/动作、运行时 64/128/192/256 尺寸 | ✅ |
| `assets/chick_character_design_pack/动画序列说明书.md` | 8 组情绪动画的帧角色与序列链 | ✅ 待逐帧肉眼验收 |
| `docs/design/` `docs/reference/` | 手表 UI/UE 规范；官方硬件资料与功耗诊断记录 | ✅ |
| `宠物手表-v0.5.0.dmg` | Mac 固件刷新工具安装包（本地签名，未公证） | ✅ 已交付 |
| `tools/` | 本地工具链（venv: cmake/ninja、sdl2-prefix: SDL2） | ✅ |

## 三、常用命令

```bash
# 编译模拟器（tools/venv 提供 cmake + ninja）
cd ~/Documents/trae_projects/mini/lvgl-simulator
../tools/venv/bin/cmake --build build -j8

# 运行模拟器（需 DYLD_LIBRARY_PATH 指向本地 SDL2）
DYLD_LIBRARY_PATH=../tools/sdl2-prefix/lib ./bin/main

# 动作预览工作台（冻结日常，只循环单个动作；仅模拟器）
DYLD_LIBRARY_PATH=../tools/sdl2-prefix/lib ./bin/main --happy-demo
DYLD_LIBRARY_PATH=../tools/sdl2-prefix/lib ./bin/main --angry-demo
DYLD_LIBRARY_PATH=../tools/sdl2-prefix/lib ./bin/main --sleep-demo
```

真机固件的编译、烧录、备份与发布检查见 [FIRMWARE.md](FIRMWARE.md) 和 [发布流程](firmware/RELEASE_WORKFLOW.md)；不要用模拟器构建替代发布构建。

## 四、开发路线

| 阶段 | 内容 | 状态 |
|-|-|-|
| 环境基线 | 本地 PC 模拟器跑通（绕开 brew 沙箱，工具链本地化） | ✅ 完成 |
| 真机基线 | ESP-IDF 编译、烧录、串口日志闭环 | ✅ 2026-10-11 首次烧录；设备当前装 v1.0.0，启动检查通过，长期稳定性待验收 |
| P0 动画 | 呼吸、心跳、眨眼、待机微动、清醒日常场景动画 | ✅ 模拟器 + 真机 |
| 电源与睡眠 | 息屏、空闲动态降频、Wi-Fi 策略与退避、电池记录、外设诊断、睡眠三阶段 | ✅ 已刷入 v1.0.0；真实电池续航与唤醒待实测 |
| P1 交互 | 点击开心（已实现模拟器 + 固件源码，未刷机）；摇一摇、提示浮标 | 🔄 部分完成 |
| 情绪动画 | 8 组关键帧（生气/开心/悲伤/惊讶/害怕/害羞/得意/睡觉）与序列预览 | 🔄 关键帧待肉眼验收；生气/开心已有模拟器预览；未进固件 |
| P2 表现 | 进食、跳舞、特效粒子 | 📅 未开始 |
| 真机验收 | 触摸、连续视觉自然感、低亮度可读性、按键/息屏循环、实际续航 | 🔄 待用户逐项验收 |

## 五、关键决策记录

1. **模拟器用 LVGL master（v9.6）**，不回退到微雪官方 v9.5（已验证 v9.5 与工程配置冲突；动画 API 两版本一致，真机移植不受影响）
2. **工具链全本地化**：brew 在沙箱环境必失败（EPERM），cmake/ninja/SDL2 均装在项目 `tools/` 内，可复现
3. **宠物 UI 逻辑平台无关，但代码分两份**：模拟器和固件各保留 `pet.c/pet.h`，逻辑同源、改动需手动同步；预览专用代码与 AI 序列帧只存在于模拟器，不进固件
4. 交付文档为本地文件（用户明确要求，不用飞书在线文档）
5. **宠物形象 = 小黄鸡**（用户提供参考图）：白底转透明，运行时画布 192×192（另保留 64/128/256），LVGLImage.py 转 ARGB8888 C 数组；动画用关键帧/程序补间，贴图只管形象
6. **动画路线演变（2026-10-18 对齐）**：早期（10-11）用户认可 Lottie + ThorVG 并完成最小验证（见 [LOTTIE.md](LOTTIE.md)）；10-17 起后续新增的日常与情绪动作动画改为"清晰 PNG 关键帧 + 程序时序补间"（日常场景动画仍沿用部件贴图 + 程序补间），AI 视频/GIF 只作方向参考、不回读当素材；Lottie 保留为已验证能力，固件未引入，是否重新启用未记录。依据 [PU-058](PRODUCT_UPDATES.md) 与实际生产收敛，详见 [PU-069](PRODUCT_UPDATES.md)
7. **固件统一命名与台账（10-16）**：`pet_watch_<类型>_<版本>_<YYYYMMDD>_<标识>.bin`，存量备份按规范重命名并逐一校验 SHA256；权威清单见 [firmware/MANIFEST.md](firmware/MANIFEST.md)
8. **正式固件默认启用无损素材压缩**（`PET_PACK_RESOURCES=ON`、`PET_USE_PACKED_ASSETS=1`），原始素材构建仅作对照；刷前核对压缩开关、资源清单与分区余量（见 [RESOURCE_OPTIMIZATION.md](firmware/RESOURCE_OPTIMIZATION.md)、[RELEASE_WORKFLOW.md](firmware/RELEASE_WORKFLOW.md)）
9. **所有情绪片段首尾统一用常态帧**（PU-060）：结构为"常态帧 → 情绪关键帧序列 → 常态帧"，首尾同帧天然闭合；预览 GIF 只作节奏参考

## 六、问题排查工作法（接手必读：遇难题先按此走，不瞎琢磨）

**顺序（自上而下，命中即止）：**
1. **查项目自有文档**：README / FIRMWARE.md（踩坑清单 §3）/ LOTTIE.md / 技术实现清单 —— 多数问题已踩过、已沉淀，直接抄解法，杜绝重复犯错
2. **查官方说明**：微雪 wiki 产品页（含 FAQ）→ GitHub Demo / issue；框架层（ESP-IDF / LVGL / BSP）查乐鑫官方文档 + ESP32 Forum
3. **查社区**：GitHub 搜索 / issue、Reddit r/esp32、论坛 —— 找同款问题与现成解法
4. **自行实验**：前三步无果才动手试；仍不行就带「现象 → 已试项 → 日志」去渠道提问（GitHub issue / 论坛 / 微雪工程师）

**两条铁律：**
- **限时**：单个问题自查 ≤20 分钟无进展即换渠道或先记录挂起，禁止无限期瞎琢磨
- **沉淀**：每次解决后把「现象 → 根因 → 解法」追加进对应文档（固件相关进 FIRMWARE.md 踩坑清单），保证换工具 / 换会话无缝接力

渠道分流清单见 FIRMWARE.md §6.6（官方/社区按问题类型分流表）。

## 七、待办

- [x] P0 四动画开发完成并在模拟器验收
- [x] Lottie 路线最小验证通过（小鸡完整渲染 + 动画循环）；现保留为已验证能力，非主线
- [x] 点击开心回应（PU-046–049）：模拟器 + 固件源码已实现，待视觉验收；未刷入真机
- [x] 睡眠 v1 与既有电源优化合并刷入真机（v1.0.0，PU-044）
- [x] 电源管理、屏幕休眠、电池记录、外设诊断第一轮（v0.3–v0.4）
- [x] 固件台账与版本命名规范（10-16，[MANIFEST.md](firmware/MANIFEST.md)）
- [ ] 8 组情绪关键帧逐帧肉眼验收（生气/开心/悲伤/惊讶/害怕/害羞/得意/睡觉）
- [ ] 情绪序列接模拟器预览并按节奏调补间（生气/开心已有初版，其余待接）
- [ ] 点击开心与情绪动画刷入真机验收
- [ ] 摇一摇反应、提示浮标
- [ ] 进食、跳舞、特效粒子
- [ ] 真实电池供电续航与唤醒实测（不能由 USB 供电数据推断）

> Lottie 造型精修与嵌入日常行为已搁置：主线改为关键帧 + 程序补间，见决策 6（Lottie 能力保留，无新需求不推进）。
