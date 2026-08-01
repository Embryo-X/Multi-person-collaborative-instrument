# BLE 通讯协议规范 · Embryo-XYZ Demo

| 字段 | 内容 |
|------|------|
| 版本 | **v0.1 草案**（2026-08-01） |
| 状态 | 软件/硬件对齐用；实现前可改 UUID，改后升小版本 |
| 架构入口 | [`../architecture/system-architecture.md`](../architecture/system-architecture.md) |
| 硬件参考仓 | `git@github.com:Embryo-X/ESP32-Piano-With-LED-And-OLED.git`（本地只读克隆：`firmware-ref/`） |

---

## 1. 总则

| 项 | 约定 |
|----|------|
| 拓扑 | 手机/PC Web = **GATT Central**；每台乐器 = **GATT Peripheral** |
| 同时连接 | Demo **2** 台（打击 + 弦乐） |
| 传输窗口 | **准备阶段**下发曲目；**START 后演奏中宜静默**（不依赖实时控灯） |
| 字节序 | 多字节整数一律 **小端 little-endian** |
| 编码 | 文本字段 UTF-8；曲数据推荐 **二进制分片**（见 §5），JSON 仅作制作态 |

---

## 2. 广播与设备识别

### 2.1 广播名（Complete Local Name）

| 角色 | 广播名示例 |
|------|------------|
| 打击 Device A | `Embryo-Perc` |
| 弦乐 Device B | `Embryo-Str` |

Web 扫描时按名称前缀过滤；连接后仍写 Role 特征做二次确认。

### 2.2 服务 UUID（128-bit）

使用项目自定义 Base（固定，勿与标准 MIDI UUID 混用）：

```
Service  : 6e400001-b5a3-f393-e0a9-e50e24dcca9e   # Embryo Conductor Service
```

> 若与 Nordic UART 系冲突可在 v0.2 整体换 Base；当前先冻结便于联调。

### 2.3 特征 UUID

| 短名 | UUID | 属性 | 说明 |
|------|------|------|------|
| Role | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | Write / Read | 设备角色 |
| SongData | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | Write (无响应或 Write With Response) | 曲目分片下发 |
| Control | `6e400004-b5a3-f393-e0a9-e50e24dcca9e` | Write | 控制指令 |
| Status | `6e400005-b5a3-f393-e0a9-e50e24dcca9e` | Notify / Read | 设备状态上报 |
| Meta | `6e400006-b5a3-f393-e0a9-e50e24dcca9e` | Read | 固件版本、键位数等 |

MTU：请求 ≥ 185；单包业务载荷按 **≤ 180 字节** 设计分片。

---

## 3. Role（角色）

**Write 1 字节：**

| 值 | 含义 |
|----|------|
| `0x01` | Percussion（打击） |
| `0x02` | Strings（弦乐） |
| `0x00` | 未分配 / 清除 |

设备 Read 回显当前角色。角色决定接受哪条 track，并加载对应 keymap 维度。

---

## 4. Control（控制）

**Write 帧：**

```
offset  size  field
0       1     opcode
1       n     payload（依 opcode）
```

### 4.1 Opcode 表

| opcode | 名称 | payload | 说明 |
|--------|------|---------|------|
| `0x01` | START | `uint32 startDelayMs`（可为 0） | 收到后 `delayMs` 再以本地时钟零点开跑 |
| `0x02` | STOP | 无 | 停止发音与时间轴，保留已加载曲 |
| `0x03` | RESET | 无 | 回 STANDBY/LOADED 起点，清播放头 |
| `0x04` | SET_SOUND_PACK | `uint8 packId` | 音色包（GM program 映射表本地查） |
| `0x05` | SET_VOLUME | `uint8 volume`（0–127） | 与旋钮可并存：取最近一次设定 |
| `0x06` | SONG_BEGIN | `uint16 totalChunks` `uint32 crc32` `uint16 trackTag` | 开始收曲 |
| `0x07` | SONG_END | 无 | 收齐后校验；成功→LOADED |
| `0x10` | VIBRATO | `uint8 depth` `uint8 rate` | 可选表情；硬件未就绪可忽略 |

`trackTag`：`0x0001`=percussion，`0x0002`=strings。

### 4.2 START 同步语义

1. Web 对两台**依次**写 START（间隔尽量 < 20ms）。  
2. `startDelayMs` 建议两台相同（如 `200`），吸收写特征抖动。  
3. 设备：`t0 = millis() + startDelayMs`（或 `esp_timer`）。  
4. 演奏中不发逐音符 BLE。

---

## 5. SongData（曲目分片）

配合 Control `SONG_BEGIN` / `SONG_END`。

### 5.1 分片帧（写入 SongData）

```
offset  size  field
0       2     chunkIndex      # 从 0 起
2       2     chunkCount      # 总分片数（与 SONG_BEGIN.totalChunks 一致）
4       N     chunkPayload    # 原始字节切片
```

设备按 `chunkIndex` 拼缓冲；重复 index 覆盖；收齐后等 `SONG_END` 做 CRC。

### 5.2 缓冲内二进制布局（单轨）

整轨缓冲 = Header + Segments + Events。

**Header（16 字节）**

```
uint32 magic       # 'EMB1' = 0x31424D45
uint16 version     # 1
uint16 trackTag    # 1 perc / 2 strings
uint16 bpm
uint16 reserved
uint32 durationMs
uint16 segmentCount
uint16 eventCount
```

**Segment 记录（变长）**

```
uint16 segmentId
uint32 startMs
uint32 endMs
uint8  keyCount          # 本段 keymap 条目数
重复 keyCount 次:
  uint8  keyId
  uint8  noteCount       # 1=单音；>1=同时发的和弦音
  uint8  notes[noteCount]  # MIDI note 或 GM 鼓号
```

**Event 记录（定长 8 字节）**

```
uint32 tMs
uint8  keyId
uint8  flags             # bit0=必须命中才算 Perfect（Demo 可忽略）
uint16 colorRgb565       # 谱面颜色；设备可忽略只用 LED 默认色
```

CRC32：对 Header+Segments+Events 整体（ISO-HDLC / zlib 多项式），见 `SONG_BEGIN.crc32`。

### 5.3 JSON 制作态

人读写用 `song.json`（见 [`song-schema.md`](./song-schema.md)）；**下发前由 Web 编码为上述二进制**。固件可不解析 JSON。

---

## 6. Status（状态 Notify）

设备主动 Notify；Web 订阅 CCCD。

**帧（固定 12 字节）**

```
uint8  state         # 0 STANDBY 1 LOADED 2 PLAYING 3 DONE 4 ERROR
uint8  flags         # bit0 曲已加载 bit1 BLE忙
uint16 segmentIndex
uint16 eventIndex
uint16 batteryMv     # 0=未知
uint8  lastError     # 0 无；1 CRC；2 分片缺失；3 内存；4 未知 opcode
uint8  reserved
uint32 playTimeMs    # PLAYING 时相对 t0；否则 0
```

建议：状态变化时立即 Notify；PLAYING 中可选每 1s 心跳（可关以省带宽）。

---

## 7. Meta（只读）

```
uint8  fwMajor, fwMinor, fwPatch
uint8  keyCount          # 逻辑键数量（A 建议 9 或 12；B 建议 4+）
uint8  hasRail           # 0/1
uint8  hasOled           # 0/1
char   buildId[8]        # 可选短哈希
```

---

## 8. 会话时序（完整）

```
Web                         Perc                         Str
 |--connect----------------->|                           |
 |--connect--------------------------------------------->|
 |--Write Role=1------------>|                           |
 |--Write Role=2---------------------------------------->|
 |--Control SONG_BEGIN------>|                           |
 |--SongData chunks--------->|                           |
 |--Control SONG_END-------->|  CRC ok → LOADED          |
 |--Control SONG_BEGIN---------------------------------->|
 |--SongData chunks------------------------------------->|
 |--Control SONG_END------------------------------------>|  LOADED
 |--Control SET_SOUND_PACK-->|                           |
 |--Control SET_SOUND_PACK------------------------------>|
 |--Control START(delay)---->|                           |
 |--Control START(delay)-------------------------------->|
 |                          |-- t0 对齐后 PLAYING -------|-- PLAYING
 |  (optional Status)       |                            |
 |--Control STOP----------->|                            |
 |--Control STOP---------------------------------------->|
```

---

## 9. 错误与重试

| 情况 | 行为 |
|------|------|
| CRC 失败 | Status.lastError=1，保持 STANDBY；Web 整轨重发 |
| 分片缺号 | SONG_END 时 error=2；Web 重发缺失 chunk 或整轨 |
| 演奏中断线 | 设备继续本地播放至 DONE 或 STOP；Web 重连后只读 Status，不重发曲除非 RESET |
| 未知 opcode | 忽略并 Status error=4 |

---

## 10. 与当前硬件仓现状的差距

参考仓 `main.ino`（commit `67b408e`）现状：

- 3 键 + RGB + OLED + `ledc` 方波喇叭  
- **尚无 BLE、无 VS1053、无时间轴引擎**  
- 逻辑为本地音阶跟灯练习，**不等于**本协议  

对齐路径：硬件仓逐步增加 BLE GATT（本 UUID）→ 收 SongData → 替换发声为 VS1053 → 段 keymap。软件仓可先 Mock Peripheral 测 Web。

---

## 11. 修订

| 版本 | 日期 | 说明 |
|------|------|------|
| v0.1 | 2026-08-01 | 首版完整草案（#015） |
