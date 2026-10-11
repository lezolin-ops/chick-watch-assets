# Lottie 矢量动画路线（电子宠物）

> 决策背景：用户否掉"纯程序化参数动画"（"只能算动画引擎做的"）与行为系统 v3（"作废吧"），
> 认可 **Lottie（JSON 矢量动画）+ LVGL 内置 ThorVG 播放** 为动画主路线。
> 本文档沉淀手写 Lottie 被 ThorVG 渲染的全部兼容规范与调试经验，供任何工具无缝接力。

## 一、技术栈与开关

| 项 | 值 |
|-|-|
| 播放器 | LVGL `lv_lottie`（内部走 ThorVG） |
| 配置开关 | `lv_conf.h`：`LV_USE_THORVG=1`、`LV_USE_LOTTIE=1`、`LV_USE_RLOTTIE=0`（rlottie 未用） |
| 画布 | 192×192（按真机 410×502 居中放大，`lv_obj_align(..., LV_ALIGN_CENTER, 0, 50)`） |
| buffer | **必须手动分配**：`lv_lottie_set_buffer(obj, w, h, buf)`，格式 ARGB8888，大小 = w×h×4，需 8 字节对齐 |
| 加载 | `lv_lottie_set_src_data(obj, data, len)` 传 C 数组（勿用文件路径方式，嵌入式无文件系统依赖） |
| 动画 | `lv_lottie_get_anim(obj)` 拿 `lv_anim_t *`，默认无限循环；帧率由 JSON `fr` 决定 |

## 二、手写 Lottie JSON 的 ThorVG 兼容规范（核心）

**警告：ThorVG 对字段缺失是"静默透明化"而不是报错**——JSON 解析成功、动画时间正常推进，但渲染结果全透明（opaque_px=0）。
鉴别信号：`anim time` 在走 + `opaque_px=0` ⇒ JSON 字段不完整，先补字段，不要怀疑渲染器。

### 2.1 必须齐全的字段（缺一可能全透明）

```
顶层：v fr ip op w h nm ddd assets layers meta（meta 必填，格式照官方）
图层：ddd ind ty nm sr ao bm ip op st ks shapes
形状组(gr)：ty ix np cix hd mn bm it
形状：椭圆(el)+填充(fl) 或 路径(sh)+描边(st) 均可
变换(tr)：ty nm p a s r o sk sa —— p/a/s/r/o 各带 ix，sk/sa 必填（默认 0）
关键帧：ks 属性 a=1 时，k=[{t,s,i:{x,y},o:{x,y}}]
```

### 2.2 最小可用模板（官方字段全集，照抄改值即可）

```json
{
  "v": "5.5.7", "fr": 30, "ip": 0, "op": 60, "w": 192, "h": 192,
  "nm": "chick", "ddd": 0, "assets": [],
  "meta": {"g": "LottieFiles AE 1.0.0", "a": "", "k": "", "d": "", "tc": ""},
  "layers": [
    {
      "ddd": 0, "ind": 1, "ty": 4, "nm": "body", "sr": 1, "ao": 0,
      "bm": 0, "ip": 0, "op": 60, "st": 0,
      "ks": {
        "o": {"a": 0, "k": 100, "ix": 11},
        "r": {"a": 0, "k": 0, "ix": 10},
        "p": {"a": 0, "k": [96, 128, 0], "ix": 2},
        "a": {"a": 0, "k": [0, 0, 0], "ix": 1},
        "s": {"a": 0, "k": [100, 100, 100], "ix": 6}
      },
      "shapes": [{
        "ty": "gr", "ix": 1, "np": 4, "cix": 2, "hd": false,
        "mn": "ADBE Vector Group", "nm": "g", "bm": 0, "it": [
          {"ty": "el", "ix": 1, "nm": "e", "p": {"a": 0, "k": [0, 0]}, "s": {"a": 0, "k": [118, 96]}},
          {"ty": "fl", "ix": 2, "nm": "Fill", "c": {"a": 0, "k": [1.0, 0.79, 0.26, 1]}, "o": {"a": 0, "k": 100}},
          {"ty": "tr", "nm": "Transform",
           "p": {"a": 0, "k": [0, 0], "ix": 2}, "a": {"a": 0, "k": [0, 0], "ix": 1},
           "s": {"a": 0, "k": [100, 100], "ix": 3}, "r": {"a": 0, "k": 0, "ix": 6},
           "o": {"a": 0, "k": 100, "ix": 7}, "sk": {"a": 0, "k": 0, "ix": 4}, "sa": {"a": 0, "k": 0, "ix": 5}}
        ]
      }]
    }
  ]
}
```

### 2.3 关键帧写法（动画属性）

```json
"r": {"a": 1, "k": [
  {"t": 0, "s": [0],  "i": {"x": [0.33], "y": [0.33]}, "o": {"x": [0.67], "y": [0.67]}},
  {"t": 7, "s": [28], "i": {"x": [0.33], "y": [0.33]}, "o": {"x": [0.67], "y": [0.67]}},
  {"t": 14, "s": [0], "i": {"x": [0.33], "y": [0.33]}, "o": {"x": [0.67], "y": [0.67]}}
], "ix": 10}
```

## 三、ThorVG 渲染顺序（与 Lottie 规范相反）

**JSON `layers` 数组的第一个图层渲染在最顶层**，最后一个在底层。
实测：`["eyes","tuft","beak","wing_front_l","wing_front_r","body","wing_back_l","wing_back_r"]` ⇒
眼睛最顶 → 呆毛 → 喙 → 前翅 → 身体 → 后翅最底。
排列图层时按"顶 → 底"写数组；后翅（藏在身后的部件）放数组末尾。

## 四、几何经验（部件"看不见"的根因）

1. **部件必须本体伸出身体轮廓外才可见**：藏在身体轮廓内、靠旋转"露出"的部件，
   旋转角度不够时永远被上层身体盖住（翅膀踩过两次坑）。左翅椭圆中心 x=33、右翅 x=159
   （身体左缘 x=37/右缘 155）即"平时可见"。
2. **颜色对比**：部件颜色要与身体拉开（身体黄 `[1.0,0.79,0.26]`、前翅亮橙 `[0.97,0.58,0.15]`、后翅深橙 `[0.83,0.42,0.10]`），否则"画了但看不见"。
3. **锚点与旋转**：部件 `ks.p` 是图层锚点（如肩部），形状内 `tr.p` 是相对锚点偏移；旋转绕图层锚点做弧线，偏移越大摆动越明显。

## 五、调试方法（opaque_px 鉴别）

```c
/* lv_lottie 每帧渲染结果统计：opaque_px>0 且随帧变化 = 渲染正常 */
lv_draw_buf_t *db = lv_canvas_get_draw_buf(obj);
const uint8_t *p = db->data;
unsigned opaque = 0;
for (int i = 0; i < 192*192; i++) if (p[i*4+3] > 10) opaque++;
```

| 现象 | 结论 |
|-|-|
| anim time 推进 + opaque_px=0 | JSON 缺官方字段，被静默透明化 → 补字段 |
| opaque_px≈9700 且变化 | 身体+眼+喙+呆毛渲染正常（4 翅版 ≈11500~12600） |
| opaque_px 稳定不变 | 动画没动，检查关键帧/动画属性 |

> 注：`lv_lottie` 内部对象结构体不完整（私有），不要直接访问 `img->draw_buf`；
> 用公开 API `lv_canvas_get_draw_buf()` 取渲染 buffer（lv_lottie 继承 canvas）。

## 六、文件清单与生成方式

| 文件 | 说明 |
|-|-|
| `src/lottie_demo.c` / `lottie_demo.h` | Demo 入口：分配 buffer → set_src_data → 居中显示 |
| `src/lottie_chick_flap_data.h` | **当前小鸡动画 C 数组**（4 翅版，~10KB，官方字段全集） |
| `src/main.c` | 当前 `lottie_demo_create(lv_screen_active(),410,502)`，`pet_create` 被注释（临时） |

**修改→生成→运行循环**（改 JSON 用 python，勿手编 C 数组）：

```bash
# 1. python 改 /tmp/chick_flap.json（或直接改生成脚本）
# 2. 生成 header（保持此模式）：
#    json.dumps(d, separators=(',',':')) → UTF-8 bytes → 每 16 字节一行写 C 数组
#    + static const size_t xxx_len = sizeof(xxx);
# 3. 编译重启：
cd lvgl-simulator
../tools/venv/bin/cmake --build build -j8
pkill -9 -f "\./bin/main"
DYLD_LIBRARY_PATH=$PWD/../tools/sdl2-prefix/lib nohup ./bin/main > sim.log 2>&1 &
```

## 七、当前状态与下一步

- ✅ Lottie 最小验证通过：小鸡完整渲染（身体/呆毛/4 翅/眼睛/喙），呼吸+扇翅+眨眼+张喙+呆毛摆动循环播放
- ✅ 调试代码已清理，工程干净
- 📋 待选：①造型精修（翅膀更像翅膀/腮红/比例）②嵌日常行为（窝坐扇翅、点击开心、行走摆翅）③两者都做
- ⚠️ 已知缺口：`main.c` 临时切到 lottie_demo，日常动画版 `pet.c` 待二选一或整合；真机 PSRAM 容量待核验
