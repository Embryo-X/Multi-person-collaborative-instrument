# Web 谱面 ↔ 硬件灯/音 对齐说明（给两位朋友 / AI）

| 字段 | 内容 |
|------|------|
| 版本 | 2026-08-03 |
| 对照仓 tip | Multi-person `6aab37d`（`song_guide` + `led_bar`） |
| Web | Embryo 真机页公路谱 + `keyColors.js` |
| 曲数据 | zip 内 `song/believer.json` / `song_data_*.h`（**tMs+keyId 已与 Web 同源**） |

> **先读结论**：时间戳（何时亮哪颗键）数据是对齐的；你现在觉得「颜色不对、亮的长短不对」，主要是**固件定色表**和**亮灯时长 hold**与 Web 不一致，不是 JSON 写错轨。

---

## 0. 一句话诊断

| 现象 | 根因 | 数据是否同源 |
|------|------|----------------|
| 灯的**颜色**和谱面色块不一样 | 朋友 `led_bar.c` 用「彩虹 Do–Si」定色；Web 用 `keyColors.js` | ❌ 色表未统一 |
| 灯**亮得很短**，谱面色块很长 | 固件 `hold = 280ms`；Web 用 `event.durationMs`（Believer 常见 ~912ms） | ⚠️ 时刻对齐，时长策略不同 |
| 整段相对谱面**偏早/偏晚一截** | Web `START delayMs=200` + 第一颗灯 `tMs=1920`；须同 delay、同 START | ✅ 协议有 delay，实现要对齐 |
| 按键**音色**和 Web 垫盘不一致 | `song_guide` **只驱动灯**，尚未按 `keymap` 发音 | ❌ 发音未接 Believer keymap |

`song_data_perc.h` / `song_data_str.h` 前 10 条与本仓导出、与 `believer.json` events **一致**（已抽查打击轨）。

---

## 1. Zip 里原先写明白了吗？

| 话题 | 原先 zip | 本文件 |
|------|----------|--------|
| 谁用 perc / str | `DEVICE-SPLIT.md` ✅ | 补充对齐细节 |
| keyId 0-based、bit(keyId+1) | 有提及 | 再强调 |
| **Web 定色表具体 RGB** | ❌ 只说「见 keyColors.js」 | ✅ 下文抄表 |
| **固件 hold=280 vs durationMs** | ❌ | ✅ |
| **朋友彩虹色表 ≠ Web** | ❌ | ✅ |
| keymap 发音 | README 写「尚未接按键发音」 | ✅ 对照现状 |

---

## 2. 时间轴：什么已经对齐、什么没有

### 2.1 已对齐（亮哪颗 · 何时）

```
Web START → BLE Control 0x01 + delayMs(LE, 默认 200)
板端 song_guide_start(delay) → 等 delay → t0
now_ms = now - t0
若 now_ms 落在 [tMs-40, tMs+hold] → 亮 keyId
```

- 事件源：`SONG_EVENTS[] = { tMs, keyId }`（无颜色、无 duration）
- 打击第一颗：`(1920, keyId=0)` → 用户体感约 **2.1s**（1920+200）才亮，正常
- 映射：Web `keyId` 0..8 → `led_bar_show_keys` 的 `bit(keyId+1)`（灯珠下标 `keyId`）

### 2.2 未对齐（亮多久）

| | Web 公路谱 | 朋友 / embryo `song_guide.c` |
|--|------------|------------------------------|
| 时长字段 | `durationMs`（JSON 有则用，夹在 180–2000） | 常量 **`hold = 280`** |
| Believer 典型 | 常约 **912ms** 色块 | 灯只亮约 **280ms** |

所以：**同一 `tMs` 起亮，但灭得比谱面早**——看起来像「时间和谱面对不上」。

**建议（固件改一处即可）**：

```c
/* 方案 A：先拉长固定窗，接近谱面观感 */
const uint32_t hold = 900;  /* 或 912 */

/* 方案 B（更好）：头文件扩展 durationMs，与 JSON 一致 */
/* song_event_t { uint32_t tMs; uint8_t keyId; uint16_t durationMs; } */
```

Web **不必**为对齐去改成 280（谱面会变得碎）；以 JSON `durationMs` 为准改板端。

### 2.3 联调时再确认

1. 板子在 **蓝牙模式**（朋友仓 mode=3），否则主循环会清灯。  
2. 串口有 `✓ START` / `timeline running`。  
3. 打击链 `song_data_perc.h`，弦乐链 `song_data_str.h`（见 `DEVICE-SPLIT.md`）。

---

## 3. 颜色：为什么和谱面不一样

### 3.1 Web（权威 · REQ-013）

公路谱 / 垫盘：**只认** `keyId → 定色`，**不读** `event.laneColor` 来上色（JSON 里的 `laneColor` 与生成脚本同色，仅作冗余）。

**打击 `PERC_KEY_COLORS`（keyId 0–8）**

| keyId | Hex | RGB (R,G,B) | 写入 WS2812 时 GRB |
|------:|-----|-------------|-------------------|
| 0 | `#ffcc00` | 255,204,0 | 204,255,0 |
| 1 | `#00ff66` | 0,255,102 | 255,0,102 |
| 2 | `#66ccff` | 102,204,255 | 204,102,255 |
| 3 | `#aaff00` | 170,255,0 | 255,170,0 |
| 4 | `#ff9900` | 255,153,0 | 153,255,0 |
| 5 | `#ff66cc` | 255,102,204 | 102,255,204 |
| 6 | `#ff3333` | 255,51,51 | 51,255,51 |
| 7 | `#b8a0ff` | 184,160,255 | 160,184,255 |
| 8 | `#44ddaa` | 68,221,170 | 221,68,170 |

**弦乐 `STR_KEY_COLORS`（keyId 0–7）**

| keyId | Hex | RGB |
|------:|-----|-----|
| 0 | `#e8a020` | 232,160,32 |
| 1 | `#f0c040` | 240,192,64 |
| 2 | `#12b886` | 18,184,134 |
| 3 | `#20c997` | 32,201,151 |
| 4 | `#4c6ef5` | 76,110,245 |
| 5 | `#748ffc` | 116,143,252 |
| 6 | `#ae3ec9` | 174,62,201 |
| 7 | `#da77f2` | 218,119,242 |

### 3.2 朋友仓现状（`components/led_bar/led_bar.c`）

```c
/* 彩虹 Do–Si，与 Web 无关 */
static const grb_t KEY_COLORS[9] = {
  {0,255,0}, {128,255,0}, {0,255,255}, {255,0,0}, {255,0,255},
  {0,0,255}, {255,0,128}, {0,128,255}, {255,255,255},
};
```

例：Web keyId0 = 金黄 `#ffcc00`；朋友 key1 灯珠 = **红**。必然「谱面和灯不是一个色」。

另：打击/弦乐目前共用同一张彩虹表；Web 是两套表。弦乐应对 **STR** 表，且只有 0–7。

### 3.3 建议替换（打击 · 可直接粘贴）

```c
/* GRB · 与 Web keyColors.js PERC_KEY_COLORS 一致 · keyId 0→灯0 */
static const grb_t KEY_COLORS_PERC[9] = {
  {204, 255,   0}, /* 0 #ffcc00 */
  {255,   0, 102}, /* 1 #00ff66 */
  {204, 102, 255}, /* 2 #66ccff */
  {255, 170,   0}, /* 3 #aaff00 */
  {153, 255,   0}, /* 4 #ff9900 */
  {102, 255, 204}, /* 5 #ff66cc */
  { 51, 255,  51}, /* 6 #ff3333 */
  {160, 184, 255}, /* 7 #b8a0ff */
  {221,  68, 170}, /* 8 #44ddaa */
};
```

弦乐（仅 0–7；第 9 灯可保持灭）：

```c
static const grb_t KEY_COLORS_STR[8] = {
  {160, 232,  32}, /* 0 #e8a020 */
  {192, 240,  64}, /* 1 #f0c040 */
  {184,  18, 134}, /* 2 #12b886 */
  {201,  32, 151}, /* 3 #20c997 */
  {110,  76, 245}, /* 4 #4c6ef5 */
  {143, 116, 252}, /* 5 #748ffc */
  { 62, 174, 201}, /* 6 #ae3ec9 */
  {119, 218, 242}, /* 7 #da77f2 */
};
```

`DEVICE_ROLE_STR` 时用 `KEY_COLORS_STR[kid]`；Perc 用 `KEY_COLORS_PERC[kid]`。

---

## 4. 音乐 / 发音能不能和 Web 对上？

| 层 | Web | 朋友仓（6aab37d） |
|----|-----|-------------------|
| 引导灯 | events | `song_guide` + `song_data_*.h` ✅ 数据同源 |
| 按键→MIDI | `segments[].keymap` | **尚未**按 Believer keymap 接到 `audio_play_*` |
| 采样 | FluidR3 mp3（zip `samples/`） | 板内自有 piano/drum PCM，需按 keymap 重灌 |

要对上「按这键发谱面同音」：读 `believer.json` → 当前段 `keymap[keyId]` → 本地采样（见 `DEVICE-SPLIT.md` + README-HANDOFF）。**灯对齐 ≠ 音已对齐。**

---

## 5. 给 AI / 朋友的改动清单（按优先级）

### P0 — 颜色与谱面一致

- [ ] 替换 `led_bar.c` 的 `KEY_COLORS` 为上文 Web 表（Perc/Str 分支）
- [ ] 确认 `show_keys` 仍用 `bit(i+1)`，`i = keyId`（0-based）

### P0 — 亮灯时长与谱面接近

- [ ] 将 `song_guide.c` 中 `hold` 从 `280` 改为 `~900`，或按事件带 `durationMs`

### P1 — 发音

- [ ] 按键查当前段 keymap（不要用旧 score_data 乐谱模式音）
- [ ] 从 zip `samples/` 按 note/program 灌采样

### 验收

1. Web 真机页 START → 第一颗打击灯 ≈ 2.1s、颜色为金黄（`#ffcc00`）  
2. 谱面色块刚过判定线时，对应物理键同色点亮，亮的时长肉眼接近色块  
3. 弦乐台广播 `Embryo-Str`，颜色为 STR 表，不是彩虹 Do–Si  

---

## 6. 相关文件

| 文件 | 用途 |
|------|------|
| **本文件 `WEB-HW-ALIGN.md`** | 色/时/音差异与改法 |
| `DEVICE-SPLIT.md` | 打击 vs 弦乐用哪份数据 |
| `README-HANDOFF.md` | START-only 包结构 |
| Web `web/js/device/keyColors.js` | 定色权威源 |
| 朋友 `components/led_bar/led_bar.c` | 当前彩虹表 |
| 朋友 `components/song_guide/song_guide.c` | `hold=280` |
