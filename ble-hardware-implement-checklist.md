# 硬件 BLE 实现清单（转发给队友）

> **单一协议全文**：[`ble-protocol.md`](./ble-protocol.md)  
> **现状差距**：[`../dev-log/2026-08-01-ble-hardware-gap.md`](../dev-log/2026-08-01-ble-hardware-gap.md)  
> Web 已按本协议实现；固件对齐后即可真机联调。

---

## 0. 为什么现在连不上

当前 `ble_app`（zip）：广播名 `ESP32-Piano`、服务 `0x00FF`、特征 `0xFF01` 只读。  
指挥台找的是下面 **128-bit UUID + Embryo 设备名**。必须改固件，不能只改 App。

---

## 1. 必须改的广播与 UUID（复制用）

### 1.1 设备名（两台不同）

| 设备 | Complete Local Name |
|------|---------------------|
| 打击 | `Embryo-Perc` |
| 弦乐 | `Embryo-Str` |

### 1.2 GATT（全部 128-bit）

```
Service  6e400001-b5a3-f393-e0a9-e50e24dcca9e
Role     6e400002-b5a3-f393-e0a9-e50e24dcca9e   Write + Read
SongData 6e400003-b5a3-f393-e0a9-e50e24dcca9e   Write（建议 Write Without Response）
Control  6e400004-b5a3-f393-e0a9-e50e24dcca9e   Write
Status   6e400005-b5a3-f393-e0a9-e50e24dcca9e   Notify + Read
Meta     6e400006-b5a3-f393-e0a9-e50e24dcca9e   Read
```

字节序：多字节 **小端**。

---

## 2. 最小可联调里程碑（建议按序）

### M1 · 能被 Web 选中并连上

- [ ] 广播名 = `Embryo-Perc` 或 `Embryo-Str`
- [ ] 广播/扫描响应里带上 Service UUID（或至少 GATT 里存在该服务）
- [ ] Chrome 点「连接打击/弦乐」能选到设备且 `getPrimaryService` 成功

### M2 · Role + Status

- [ ] Role：写 `0x01`（打击）或 `0x02`（弦乐）；读回显
- [ ] Status：连接后 Notify 一帧（state=`STANDBY=0` 即可）

### M3 · 收曲 → LOADED

- [ ] Control `0x06` SONG_BEGIN：`uint16 totalChunks` + `uint32 crc32` + `uint16 trackTag`
- [ ] SongData：每帧 `uint16 chunkIndex` + `uint16 chunkCount` + payload
- [ ] Control `0x07` SONG_END：拼齐 → CRC32 校验 → state=`LOADED=1` 并 Notify
- [ ] 曲缓冲布局见协议 §5.2（magic `EMB1` = `0x31424D45`）

### M4 · START / STOP

- [ ] Control `0x01` START：payload `uint32 startDelayMs`；本地 `t0 = now + delay`
- [ ] Control `0x02` STOP
- [ ] 演奏中 **不必** 再收 BLE 逐音；按已下发 events 本地跑时间轴 + LED

### M5 · 发音与多键（与谱面一致）

- [ ] 段内任意键 → 查当前 segment keymap 发音（可同时多键按下）
- [ ] **同 `tMs` 多条 event** → **同时亮多颗 LED**（REQ-006）
- [ ] keymap 中 `noteCount>1` → 一键多 MIDI 音（VS1053 同时 note_on）

---

## 3. Control opcode 速查

| opcode | 名称 | payload |
|--------|------|---------|
| `0x01` | START | `uint32 delayMs` |
| `0x02` | STOP | — |
| `0x03` | RESET | — |
| `0x04` | SET_SOUND_PACK | `uint8 packId` |
| `0x05` | SET_VOLUME | `uint8 0–127` |
| `0x06` | SONG_BEGIN | `u16 chunks` `u32 crc` `u16 trackTag` |
| `0x07` | SONG_END | — |
| `0x10` | VIBRATO | `u8 depth` `u8 rate`（可先忽略） |

`trackTag`：`1`=打击，`2`=弦乐。

---

## 4. Event 与「同时多键」

每条引导事件固定 **8 字节**：

```
uint32 tMs
uint8  keyId
uint8  flags
uint16 colorRgb565
```

同一 `tMs` 出现多条 = 同一时刻多个 `keyId` 都要引导（LED/谱面）。  
**不要**把同时多键压成一条 event。

---

## 5. 联调验收（软件侧已有）

1. PC/Android **Chrome** 打开本仓 `web/`  
2. 连接两台（或先一台）→ 名称应对上  
3. 「下发曲目」→ Status 变 LOADED  
4. START → 双机本地计时；谱面开跑  
5. 故意同时按两键 → 两音同时出；谱面同 tMs 双色块  

Mock 模式可先验软件；真机必须完成 M1–M4。

---

## 6. 相对旧 `ble_app.c` 的替换要点

| 旧代码 | 改为 |
|--------|------|
| `DEVICE_NAME "ESP32-Piano"` | `Embryo-Perc` / `Embryo-Str` |
| `SVC_UUID 0x00FF` | 128-bit Service |
| 单个 `CHAR_UUID 0xFF01` Read | 五个特征 + 正确属性 |
| 无写回调 | 处理 Role / Control / SongData 写入 |
| 无曲缓冲 | SONG_* + CRC + 时间轴任务 |

完整字段与时序以 [`ble-protocol.md`](./ble-protocol.md) 为准；本文是落地清单。
