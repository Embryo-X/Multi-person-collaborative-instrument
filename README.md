# 多人协作硬件乐器 — 固件项目

基于 ESP32-S3 的固件，当前实现：**按键控制 OLED 文字切换**。

---

## 项目结构

```
├── CMakeLists.txt              # 顶层 CMake（ESP-IDF 项目入口）
├── sdkconfig.defaults           # 默认 Kconfig 配置
├── main/
│   ├── CMakeLists.txt
│   └── main.c                  # ★ 主程序（初始化 + 主循环）
├── components/
│   ├── oled/                    # ★ OLED 显示组件（SSD1306 128x64 I²C）
│   │   ├── oled.h              #    - 引脚、I²C 地址在此修改
│   │   └── oled.c              #    - 含 6x8 ASCII 完整字库
│   ├── button/                  # ★ 按键组件（GPIO + 软件消抖）
│   │   ├── button.h            #    - 引脚、消抖时间在此修改
│   │   └── button.c            #    - 状态机实现（空闲→消抖→按下→消抖→释放）
│   ├── wifi_app/               # ◇ Wi-Fi 联网（占位，待实现）
│   ├── audio/                   # ◇ 音频输出（占位，待实现）
│   └── ble_app/                # ◇ BLE 蓝牙通信（占位，待实现）
└── README.md
```

★ = 已实现    ◇ = 预留框架

---

## 开发环境搭建

### 1. 安装 ESP-IDF

推荐使用 **VSCode + ESP-IDF 插件**（最省心）：

- [官方安装指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html)

或者命令行安装：

```bash
# Windows (PowerShell 管理员模式)
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
install.bat esp32s3
```

### 2. 设置芯片目标

```bash
idf.py set-target esp32s3
```

### 3. 配置 I²C 引脚（可选）

如果默认引脚不符，修改以下文件即可：

- **OLED 引脚**：[components/oled/oled.h](components/oled/oled.h) 顶部 `#define`
- **按键引脚**：[components/button/button.h](components/button/button.h) 顶部 `#define`
- **文字内容**：[main/main.c](main/main.c) 中 `MODE_TEXTS[]` 数组

### 4. 编译 & 烧录

```bash
# 先连接 ESP32-S3 到电脑（Type-C 数据线）

# 编译
idf.py build

# 烧录（自动选择串口）
idf.py -p COM3 flash        # Windows，COM3 替换为实际端口号
# 或
idf.py -p /dev/ttyUSB0 flash # Linux/Mac

# 烧录 + 打开串口监视器（看日志）
idf.py -p COM3 flash monitor

# 退出监视器：Ctrl + ]
```

---

## 硬件接线

```
ESP32-S3              SSD1306 OLED (0.96")
---------             --------------------
GPIO 40 (SCL)   ───   SCL
GPIO 41 (SDA)   ───   SDA
3.3V            ───   VCC
GND             ───   GND

ESP32-S3              按键
---------             --------
GPIO 1           ───  按键一脚
GND              ───  按键另一脚（按下时接地）
```

---

## 修改文字内容

打开 [main/main.c](main/main.c)，找到这一段：

```c
static const char *MODE_TEXTS[] = {
    "Mode A: Hello World",    // 模式 0 显示的文字 ← 改成你想要的
    "Mode B: ESP32-S3 OK",    // 模式 1 显示的文字 ← 改成你想要的
};
```

改完后重新编译烧录即可。每行最多 **21 个 ASCII 字符**（超过会被截断）。

---

## 后续扩展计划

| 功能 | 所在组件 | 状态 |
|------|---------|------|
| 按键 → OLED 文字切换 | `oled` + `button` | ✅ 已完成 |
| Wi-Fi 联网 | `wifi_app/` | ◇ 待实现 |
| 音频输出 | `audio/` | ◇ 待实现 |
| BLE 手机通信 | `ble_app/` | ◇ 待实现 |
| OTA 远程升级 | `wifi_app/` | ◇ 待实现 |

---

## 常见问题

**Q: 编译报错 "Components list contains duplicate"？**
A: 删除项目根目录的 `build/` 文件夹，重新 `idf.py build`。

**Q: 烧录后 OLED 不显示？**
A: 检查 I²C 接线是否正确（SDA→GPIO41, SCL→GPIO40），以及 SSD1306 的 VCC/GND 是否接对。

**Q: 按键没反应？**
A: 确认按键一脚接 GPIO1，另一脚接 GND。按键按下时 GPIO1 被拉低。

**Q: 如何确认烧录成功？**
A: 用串口监视器查看日志：`idf.py -p COM3 monitor`，启动时应看到：
```
I (xxx) main: ========================================
I (xxx) main:   按键控制 OLED 文字切换 - 启动中...
I (xxx) main: ========================================
I (xxx) main: 当前模式: 0 → "Mode A: Hello World"
I (xxx) main: 初始化完成，进入主循环...
I (xxx) main: 按键触发！切换到模式: 1 → "Mode B: ESP32-S3 OK"
```
