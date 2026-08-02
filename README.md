# 多人协作硬件乐器 — ESP32-S3 固件

基于 ESP32-S3 的多模式电子乐器控制器，带 **TFT 彩屏、真实钢琴采样发声、矩阵键盘、电容触摸、WS2812 灯带**，面向"多人协作演奏"项目。

> BLE 通讯协议见 [`ble-protocol.md`](ble-protocol.md)，落地清单见 [`ble-hardware-implement-checklist.md`](ble-hardware-implement-checklist.md)

---

## 功能总览

| 功能 | 说明 |
|------|------|
| **4 种模式** | 架子鼓 / 电子琴 / 乐谱 / 蓝牙（GPIO4 切换） |
| **13 个音符** | Do~高La（矩阵键盘 9 键 + 触摸 4 键） |
| **真实钢琴音** | 88 键钢琴录音采样 + 变调，I2S 输出到 MAX98357A 功放 |
| **多音复音** | 8 声道混音，支持同时按下多个键（和弦） |
| **TFT 彩屏** | NV3007 142×428，竖排文字显示 |
| **WS2812 灯带** | 13 灯珠对应 13 音，弹奏点亮 + 乐谱模式提示 |
| **音量调节** | 电位器（ADC），屏幕右上角显示 |
| **蓝牙** | 默认关闭（`BLE_ENABLED` 宏开启） |

---

## 硬件接线

### 屏幕（NV3007 142×428，8 脚）

| 屏引脚 | → ESP32 | 说明 |
|--------|---------|------|
| GND | GND | 地 |
| VCC | 3.3V | 电源 |
| SCL | GPIO2 | SPI 时钟 |
| MOS | GPIO1 | SPI 数据（=SDA） |
| RES | GPIO41 | 复位 |
| DC | GPIO42 | 命令/数据 |
| CS | GPIO40 | 片选 |
| BLK | GPIO9 | 背光（高电平亮） |

### 音频（MAX98357A）

| 引脚 | ESP32 |
|------|-------|
| BCLK | GPIO48 |
| LRCLK | GPIO45 |
| DIN | GPIO47 |
| 音量电位器 | GPIO5（ADC1） |

### 矩阵键盘（3×3 = 9 键）

```
       C35    C36    C37
R38     Do     Re     Mi
R39     Fa     Sol    La
R21     Si    高Do   高Re
```
- 行：GPIO38 / GPIO39 / GPIO21
- 列：GPIO35 / GPIO36 / GPIO37

### 触摸（4 键，高音区）

| 触摸通道 | 引脚 | 音符 |
|----------|------|------|
| TOUCH11 | GPIO11 | 高Mi |
| TOUCH12 | GPIO12 | 高Fa |
| TOUCH13 | GPIO13 | 高Sol |
| TOUCH14 | GPIO14 | 高La |

### 其他

| 功能 | 引脚 |
|------|------|
| WS2812 灯带 | GPIO20 |
| 模式切换键 | GPIO4 |
| Do/Re LED | GPIO15 / GPIO16 |

---

## 音符按键布局（13 音）

| key | 音符 | 输入 |
|-----|------|------|
| 1~9 | Do 高Re | 矩阵键盘 |
| 10~13 | 高Mi 高La | 触摸 |

> 高Fa/高Sol/高La 用高Mi 采样变速播放（变调），音高正确、音色略变。

---

## 四种模式

| # | 模式 | 行为 |
|---|------|------|
| 0 | 架子鼓 | 静音（预留） |
| 1 | 电子琴 | 弹奏发声 + 灯带跟随，支持和弦 |
| 2 | 乐谱 | 按谱面跟弹，灯带提示该按的键，完成显示"完成" |
| 3 | 蓝牙 | 显示连接状态（蓝牙默认关闭） |

按 **GPIO4** 循环切换。

---

## 开发环境

- ESP-IDF **v5.5.4**，目标芯片 **esp32s3**
- 分区表：自定义 `partitions.csv`（4MB app，容纳 1.3MB 采样）

```bash
idf.py set-target esp32s3
idf.py -p COM3 flash monitor
```

> 或双击 [`tools/build.bat`](tools/build.bat) 一键编译烧录。

---

## 项目结构

```
├── CMakeLists.txt              # 顶层 CMake
├── partitions.csv               # 自定义分区表（4MB app）
├── sdkconfig.defaults           # 默认配置
├── main/
│   └── main.c                  # 主程序（4 模式主循环）
├── components/
│   ├── nv3007/                  # NV3007 屏幕驱动（142×428）
│   ├── oled/                    # 绘制层：帧缓冲 + 中文/ASCII 竖排字库
│   ├── audio/                   # 音频：I2S + 采样回放 + 变调
│   │   └── piano_samples.c     # ★ 10 个钢琴采样数组（勿删）
│   ├── keypad/                  # 3×3 矩阵键盘
│   ├── touch/                   # 电容触摸 4 键（高音区）
│   ├── led_bar/                 # WS2812 灯带 13 灯
│   ├── ble_app/                 # BLE（默认关闭）
│   └── button/                  # 模式切换键
└── tools/
    ├── build.bat                # 一键编译烧录
    ├── convert_mp3_to_samples.py # MP3 → 钢琴采样 C 数组
    └── gen_piano_samples.py     # 物理建模生成采样（备选）
```

---

## 修改指南

| 想改什么 | 位置 |
|----------|------|
| 屏幕方向/引脚 | [`components/nv3007/nv3007.h`](components/nv3007/nv3007.h) |
| 音符频率 | [`components/audio/audio.h`](components/audio/audio.h) |
| 乐谱 | [`main/main.c`](main/main.c) 的 `SCORE[]` 数组 |
| 触摸灵敏度 | [`components/touch/touch.c`](components/touch/touch.c) 的 `TOUCH_THRESH` |
| 蓝牙开关 | [`main/main.c`](main/main.c) 的 `BLE_ENABLED` |
| 灯带颜色 | [`components/led_bar/led_bar.c`](components/led_bar/led_bar.c) |

---

## 常见问题

**Q: 编译报错 `app partition is too small`？**
A: 固件含 1.3MB 采样，需自定义分区表。确认 `sdkconfig` 里 `CONFIG_PARTITION_TABLE_CUSTOM=y`，或删 `sdkconfig` 从默认重新生成。

**Q: 屏幕白屏/花屏？**
A: NV3007 屏幕初始化较复杂。先确认接线（8 脚），再查 `nv3007.h` 的 MADCTL 方向配置。

**Q: 触摸误触发？**
A: 检查触摸引脚接线是否稳定。`TOUCH_THRESH`（灵敏度）和基线调整逻辑在 `touch.c`。

**Q: 蓝牙怎么开？**
A: `main/main.c` 里 `#define BLE_ENABLED 1`，重新编译烧录。

---

## 后续扩展计划

| 功能 | 状态 |
|------|------|
| 4 模式 + 电子琴 + 乐谱 | ✅ |
| 真实钢琴采样 + 变调 | ✅ |
| 多音复音（和弦） | ✅ |
| 触摸 4 键高音区 | ✅ |
| WS2812 灯带跟随 | ✅ |
| 蓝牙连接 | ◇ 默认关，可开启 |
| 架子鼓/吉他音色 | ◇ 待实现 |
