# 电子宠物手表（pet-watch）

> 载体：微雪 ESP32-S3-Touch-AMOLED-2.06（410×502 AMOLED 触控屏）
> 框架：LVGL v9（ESP-IDF 真机路线 + LVGL PC 模拟器先行开发）
> 开发模式：AI Agent 开发（用户为产品经理，负责产品文档与验收，不提交代码）
> 状态：**真机阶段——固件已烧录真机（2026-10-11），真机验收中**（模拟器环境已打通）

硬件规格见 [HARDWARE.md](HARDWARE.md)，它是模拟器分辨率、素材尺寸和真机移植的统一基线。

## 一、项目目标

在微雪 2.06 手表开发板上做一个"电子宠物"——固件内置应用，程序化动画为主、精灵图为辅，可实时响应触摸与六轴感应，具备生命感与互动感。

## 二、目录结构

| 目录/文件 | 说明 | 状态 |
|-|-|-|
| `README.md` | 项目总览（本文档） | ✅ |
| `HARDWARE.md` | 用户确认的手表硬件配置与项目约束 | ✅ |
| `电子宠物动画系统技术实现清单.md` | 技术清单（状态机/动画分级/精灵图/执行顺序/验证方式） | ✅ |
| `lvgl-simulator/` | LVGL PC 模拟器工程（编译产物 `bin/main`） | ✅ 已跑通 |
| `lvgl-simulator/src/pet.c` `pet.h` | **宠物核心模块（平台无关，真机直接复用）** | ✅ P0 完成 |
| `lvgl-simulator/src/pet_chick.c` | 小黄鸡贴图（LVGLImage.py 生成，ARGB8888 C 数组） | ✅ |
| `LOTTIE.md` | **Lottie 动画路线专档**：ThorVG 兼容字段规范、渲染顺序、几何/调试经验、生成流程 | ✅ 验证通过 |
| `lvgl-simulator/src/lottie_demo.c` + `lottie_chick_flap_data.h` | Lottie 小鸡动画 Demo（lv_lottie + ThorVG 播放） | ✅ 运行中 |
| `tools/` | 本地工具链（venv: cmake/ninja、sdl2-prefix: SDL2） | ✅ |
| `firmware/` | 真机 ESP-IDF 工程（基于官方 02_lvgl_demo_v9 + pet 素材 156 文件） | ✅ 已烧录真机（2026-10-11），验收中 |
| `FIRMWARE.md` | **真机移植通关记录**：环境基线/编译命令/全部踩坑与解法（接手必读） | ✅ |
| `assets/` | 宠物形象素材：原始参考图、完整角色设定包、透明母版与 64/128/192/256 运行时尺寸 | ✅ |
| `assets/chick_character_design_pack/` | 转面、表情、动作、部件拆分、补充角度、宠物状态及 410×502 显示测试 | ✅ |
| `assets/runtime/chick/` | 面向 LVGL 的透明 PNG：`sprite_front_64/128/192/256.png` | ✅ |

## 三、常用命令

```bash
# 编译模拟器（tools/venv 提供 cmake + ninja）
cd ~/Documents/trae_projects/mini/lvgl-simulator
../tools/venv/bin/cmake --build build -j8

# 运行模拟器（需 DYLD_LIBRARY_PATH 指向本地 SDL2）
DYLD_LIBRARY_PATH=../tools/sdl2-prefix/lib ./bin/main
```

## 四、开发路线（来自技术实现清单）

| 阶段 | 内容 | 状态 |
|-|-|-|
| 阶段零：环境基线 | 本地 PC 模拟器跑通（绕开 brew 沙箱，工具链本地化） | ✅ 完成 |
| 阶段二：P0 动画 | 呼吸、心跳、眨眼、待机微动（小黄鸡形象贴图 + 程序化动画） | ✅ 完成，模拟器运行中 |
| 阶段三：P1 交互 | 触摸开心、摇一摇、状态切换、提示浮标 | 📅 待排期 |
| 阶段四：P2 表现 | 进食、走路、跳舞、特效粒子（需精灵图） | 📅 待排期 |
| 阶段一：真机基线 | ESP-IDF 固件编译跑通 + 串口日志闭环 | ✅ 固件已烧录真机（2026-10-11），验收中 |

## 五、关键决策记录

1. **模拟器用 LVGL master（v9.6）**，不回退到微雪官方 v9.5（已验证 v9.5 与工程配置冲突；动画 API 两版本一致，真机移植不受影响）
2. **工具链全本地化**：brew 在沙箱环境必失败（EPERM），cmake/ninja/SDL2 均装在项目 `tools/` 内，可复现
3. **宠物 UI 代码平台无关**：`pet.c/pet.h` 只依赖 LVGL，模拟器（SDL）与真机（ESP-IDF）共用同一份代码，仅屏幕尺寸参数不同
4. 交付文档为本地文件（用户明确要求，不用飞书在线文档）
5. **宠物形象 = 小黄鸡**（用户提供参考图）：白底转透明 + 缩放到 128×128 + LVGLImage.py 转 ARGB8888 C 数组（`src/pet_chick.c`）；动画全部程序化（呼吸/心跳/眨眼/微动），贴图只管形象
6. **动画主路线 = Lottie + ThorVG**（用户否掉纯程序化动画，认可 Lottie）：`lv_lottie` 播放手写 JSON（C 数组内嵌）；手写 JSON 必须带官方字段全集（bm/ix/np/cix/hd/mn/sa/sk/meta），缺字段会被 ThorVG **静默渲染成全透明**；图层数组第一个渲染在最顶；细节见 [LOTTIE.md](LOTTIE.md)

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
- [x] Lottie 路线最小验证通过（小鸡完整渲染 + 动画循环）
- [ ] Lottie 造型精修（翅膀更像翅膀/腮红/按素材比例）
- [ ] Lottie 动画嵌入日常行为（窝坐扇翅、点击开心、行走摆翅）
- [ ] P1 交互开发（触摸开心/摇一摇反应）——模拟器先行
- [ ] 宠物形象扩展：闭眼帧 / 表情帧（AI 生成，规格 128×128 透明底）
- [x] 开发板到货 → 建 `firmware/` ESP-IDF 工程 → 移植 pet.c + 贴图（156 文件）→ 固件编译完成（pet_watch.bin）
- [ ] 真机烧录验证：小鸡画面 + 触摸响应 + 串口日志（烧录步骤/避坑见 FIRMWARE.md §2、§6）
