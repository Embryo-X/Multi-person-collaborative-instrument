# 多人协作硬件乐器 — ESP32-S3 固件

基于 ESP32-S3 的多模式电子乐器控制器，带 **TFT 彩屏、真实钢琴采样发声、矩阵键盘、电容触摸、BLE 蓝牙**，面向"多人协作演奏"项目。

> BLE 通讯协议见 [`ble-protocol.md`](ble-protocol.md)，落地清单见 [`ble-hardware-implement-checklist.md`](ble-hardware-implement-checklist.md)

---

## 功能总览

| 功能 | 说明 |
|------|------|
| **5 种模式** | 架子鼓 / 吉他 / 电子琴 / 乐谱 / 蓝牙（GPIO4 按键切换） |
| **10 个音符** | Do~高Mi（矩阵键盘 9 键 + 触摸 1 键） |
| **真实钢琴音** | 88 键钢琴录音采样（16bit/44.1kHz），I2S 输出到 MAX98357A 功放 |
| **TFT 彩屏** | ST7735 128×160，中文显示当前模式/音符/音量 |
| **音量调节** | 电位器（ADC），屏幕右上角实时显示百分比 |
| **BLE 蓝牙** | Embryo 协作协议（设备名 `Embryo-Perc`），连接状态屏幕显示 |

---

## 硬件接线

### 核心引脚（都在各组件 `.h` 文件顶部 `#define`）

| 外设 | 引脚 | 说明 |
|------|------|------|
| **TFT 屏幕** | SCLK=2, SDA=1, DC=42, RST=41, CS=40 | ST7735 128×160 SPI |
| **音频功放** | BCLK=48, LRCLK=45, DIN=47 | MAX98357A I2S |
| **音量电位器** | GPIO5 | ADC1_CH4 |
| **模式切换键** | GPIO4 | 按下切换模式 |
| **Do/Re LED** | GPIO15/16 | 乐谱模式提示灯 |

### 矩阵键盘（3×3 = 9 键）

```
       C35    C36    C37
R38     Do     Re     Mi
R39     Fa     Sol    La
R21     Si    高Do   高Re
```

- **行**（输出，逐行拉低扫描）：GPIO38 / GPIO39 / GPIO21
- **列**（输入，上拉）：GPIO35 / GPIO36 / GPIO37
- 按键一端接行线，另一端接列线

### 电容触摸

| 触摸通道 | 引脚 | 音符 |
|----------|------|------|
| TOUCH14 | GPIO14 | 高Mi |

---

## 音符按键布局

| key 编号 | 音符 | 输入方式 |
|----------|------|---------|
| 1 | Do (262Hz) | 矩阵 R38×C35 |
| 2 | Re (294Hz) | 矩阵 R38×C36 |
| 3 | Mi (330Hz) | 矩阵 R38×C37 |
| 4 | Fa (349Hz) | 矩阵 R39×C35 |
| 5 | Sol (392Hz) | 矩阵 R39×C36 |
| 6 | La (440Hz) | 矩阵 R39×C37 |
| 7 | Si (494Hz) | 矩阵 R21×C35 |
| 8 | 高Do (523Hz) | 矩阵 R21×C36 |
| 9 | 高Re (587Hz) | 矩阵 R21×C37 |
| 10 | 高Mi (659Hz) | 触摸 GPIO14 |

---

## 五种模式

| # | 模式 | 行为 |
|---|------|------|
| 0 | 架子鼓 | 无声（预留） |
| 1 | 吉他 | 无声（预留） |
| 2 | 电子琴 | 按键→弹奏对应音符，屏幕显示音符名 |
| 3 | 乐谱 | 按谱面顺序按键跟弹，Do/Re LED 提示，完成显示"完成" |
| 4 | 蓝牙 | 显示连接状态（已连接/等待中），不发音 |

按 **GPIO4** 循环切换模式。

---

## 开发环境搭建

### 1. 安装 ESP-IDF

本项目使用 **ESP-IDF v5.5.4**。推荐两种方式：

- **VSCode + ESP-IDF 插件**：[官方安装指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html)
- **命令行**：下载 [ESP-IDF Windows 离线安装包](https://dl.espressif.com/dl/esp-idf/)

### 2. 编译 & 烧录

Windows 打开 **ESP-IDF 5.5 CMD** 终端：

```bash
cd 项目目录
idf.py set-target esp32s3     # 首次只需一次
idf.py build                  # 编译
idf.py -p COM3 flash monitor  # 烧录 + 打开串口日志（COM3 换实际端口）
```

> 或者双击 [`tools/build.bat`](tools/build.bat) 一键编译烧录。

退出串口监视器：`Ctrl + ]`

### 3. 配置芯片目标

`idf.py set-target esp32s3` 后，项目会使用 `sdkconfig.defaults` 自动启用 BLE 蓝牙。

---

## 项目结构

```
├── CMakeLists.txt              # 顶层 CMake
├── partitions.csv               # 自定义分区表（4MB app）
├── sdkconfig.defaults           # 默认配置（BLE 启用等）
├── main/
│   └── main.c                  # 主程序（初始化 + 5 模式主循环）
├── components/
│   ├── audio/                   # 音频：I2S + 钢琴采样回放
│   │   └── piano_samples.c     # ★ 10 个真实钢琴采样 C 数组（勿删，编译需要）
│   ├── ble_app/                 # BLE 蓝牙（Embryo-Perc 协议）
│   ├── oled/                    # ST7735 TFT 显示 + 中文/ASCII 字库
│   ├── keypad/                  # 3×3 矩阵键盘扫描
│   ├── touch/                   # 电容触摸（高Mi）
│   ├── button/                  # 模式切换按键
│   ├── led/                     # LED（预留）
│   └── wifi_app/                # Wi-Fi（预留）
└── tools/
    ├── build.bat                # 一键编译烧录
    ├── convert_mp3_to_samples.py # MP3 → 钢琴采样 C 数组
    └── gen_piano_samples.py     # 物理建模生成采样（备选）
```

> **注意**：`piano_samples.c`（约 5MB）是真实钢琴录音转换的采样数组，编译必需。源素材 MP3（`88钢琴按键音/`）不入库。

---

## 修改指南

| 想改什么 | 改哪里 |
|----------|--------|
| 屏幕引脚 | [`components/oled/oled.h`](components/oled/oled.h) |
| 音频引脚/音量 | [`components/audio/audio.h`](components/audio/audio.h) |
| 音符频率 | [`components/audio/audio.h`](components/audio/audio.h) 的 `NOTE_*` 宏 |
| 矩阵行列引脚 | [`components/keypad/keypad.c`](components/keypad/keypad.c) |
| 触摸通道 | [`components/touch/touch.c`](components/touch/touch.c) |
| 模式切换/按键逻辑 | [`main/main.c`](main/main.c) |
| BLE 设备名/UUID | [`components/ble_app/ble_app.c`](components/ble_app/ble_app.c) |

---

## 常见问题

**Q: 编译报错 `app partition is too small`？**
A: 固件含采样数据约 2MB，需用自定义分区表。确认 `sdkconfig` 里 `CONFIG_PARTITION_TABLE_CUSTOM=y`，或删除 `sdkconfig` 让它从 `sdkconfig.defaults` 重新生成。

**Q: 按键出来是重复的 / 多出音？**
A: 先看串口日志（`flash monitor`）按每个键，观察 `弹奏: xxx (key=N, row=R, col=C)`：
- 若**按一个键出现两个不同的 key/row/col** → 硬件接线问题（按键短路或行/列线共用），检查新增的 GPIO21 第三行接线。
- 若**松开后多出 key=10** → 触摸误触发，检查 GPIO14 是否被金属物靠近。

**Q: 烧录后屏幕不显示？**
A: 检查 ST7735 接线（SCLK=2, SDA=1, DC=42, RST=41, CS=40）。注意是 **SPI 屏**不是 I²C。

**Q: 没声音？**
A: 确认模式在"电子琴"（默认），按矩阵键应有音。检查 MAX98357A 接线（BCLK=48, LRCLK=45, DIN=47），以及音量电位器是否调大。

**Q: 怎么确认蓝牙工作？**
A: 切到"蓝牙模式"，屏幕应显示"蓝牙模式/等待中..."。用手机/Chrome 扫描 BLE，应看到设备名 **Embryo-Perc**，连接后屏幕变"已连接"。

---

## 后续扩展计划

| 功能 | 状态 |
|------|------|
| 5 模式 + 电子琴 + 乐谱 | ✅ 完成 |
| 真实钢琴采样 | ✅ 完成 |
| BLE 连接（Embryo-Perc） | ✅ 完成（收曲/时间轴待实现） |
| 架子鼓/吉他音色 | ◇ 待实现 |
| 蓝牙收曲 + 时间轴演奏 | ◇ 待实现 |
| LED 灯带引导 | ◇ 待实现 |
