#!/usr/bin/env python3
"""MIDI → 分段一键乐谱
把 MIDI 所有轨音符按"音符间隙"分段：
- 段内多个音符（快速音/和弦）→ 自动连续播放
- 每个段一个代表键，用户按一次就播完段内所有音
输出:
  SCORE[]         每段代表键（1~9）
  SEG_DUR_MS[]    每段时长(ms)
  SEG_NOTE_START[] 每段在 SEG_NOTES 的起始索引
  SEG_NOTE_COUNT[] 每段音符数
  SEG_NOTES_KEY[]  段内音符键号（按段顺序）
  SEG_NOTES_OFFSET[] 段内音符相对段开始的 ms
"""
import sys, struct

def read_varint(data, i):
    v = 0
    while True:
        b = data[i]; i += 1
        v = (v << 7) | (b & 0x7F)
        if not (b & 0x80): break
    return v, i

def parse(path):
    with open(path, 'rb') as f:
        data = f.read()
    fmt, ntrks, div = struct.unpack('>HHH', data[8:14])
    all_notes = []   # (tick, note)
    tempos = []      # (tick, tempo_us)
    i = 14
    for trk in range(ntrks):
        if data[i:i+4] != b'MTrk': continue
        length = struct.unpack('>I', data[i+4:i+8])[0]
        end = i + 8 + length
        i += 8
        active = {}
        t = 0
        running = 0
        while i < end:
            delta, i = read_varint(data, i)
            t += delta
            b = data[i]
            if b & 0x80:
                running = b; i += 1
            status = running
            msg = status & 0xF0
            if msg == 0x90:
                note, vel = data[i], data[i+1]; i += 2
                if vel > 0: active[note] = t
                else:
                    if note in active: all_notes.append((active[note], note)); del active[note]
            elif msg == 0x80:
                note = data[i]; i += 2
                if note in active: all_notes.append((active[note], note)); del active[note]
            elif status == 0xFF:
                meta = data[i]; i += 1
                l, i = read_varint(data, i)
                if meta == 0x51 and l == 3:
                    tempos.append((t, struct.unpack('>I', b'\x00'+data[i:i+3])[0]))
                i += l
            elif status in (0xF0, 0xF7):
                l, i = read_varint(data, i); i += l
            else:
                if msg in (0xC0, 0xD0): i += 1
                else: i += 2
        for note, st in active.items():
            all_notes.append((st, note))
        i = end
    return all_notes, tempos, div

PITCH_TO_KEY = {0:1, 1:1, 2:2, 3:3, 4:3, 5:4, 6:4, 7:5, 8:5, 9:6, 10:7, 11:7}

def main(path):
    notes, tempos, div = parse(path)
    if not notes: print("无音符"); return
    notes.sort(key=lambda x: x[0])
    if not tempos: tempos = [(0, 500000)]
    def tempo_at(tick):
        tv = tempos[0][1]
        for tt, v in tempos:
            if tt <= tick: tv = v
            else: break
        return tv

    # 转成 (ms, note)
    evts = [(int(tick / div * tempo_at(tick) / 1000), n) for tick, n in notes]
    evts.sort(key=lambda x: x[0])

    # 按音符间隙分段：间隔 > 250ms 则开新段
    SEG_GAP = 250
    DUP_GAP = 150    # 段内同键音符间隔 <150ms 视为重复，只留一个
    segments = []   # [(start_ms, [(offset_ms, key)])]
    cur = None
    prev_ms = None
    for ms, note in evts:
        key = PITCH_TO_KEY[note % 12]
        if cur is None or (prev_ms is not None and ms - prev_ms > SEG_GAP):
            cur = [ms, []]
            segments.append(cur)
        # 段内同键去重：避免同一音反复触发（不同音高可能映射到同一键）
        if cur[1] and cur[1][-1][1] == key and (ms - cur[0] - cur[1][-1][0]) < DUP_GAP:
            pass
        else:
            cur[1].append((ms - cur[0], key))
        prev_ms = ms

    # 每段时长：到下一段开始 或 段内最大offset+400
    seg_keys, seg_dur, note_keys, note_offsets, seg_start, seg_count = [], [], [], [], [], []
    for idx, (start, sn) in enumerate(segments):
        end = segments[idx+1][0] if idx+1 < len(segments) else start + 4000
        seg_keys.append(sn[0][1])          # 代表键 = 段内第一个音
        seg_dur.append(max(500, end - start))
        seg_start.append(len(note_keys))
        seg_count.append(len(sn))
        for off, k in sn:
            note_offsets.append(off)
            note_keys.append(k)

    print(f"段数: {len(segments)}, 总音符: {len(note_keys)}, 时长 {segments[-1][0]/1000:.0f} 秒")

    def fmt(vals, name, typ):
        out = f"static const {typ} {name}[] = {{\n"
        for i in range(0, len(vals), 16):
            out += "    " + ",".join(str(x) for x in vals[i:i+16]) + ",\n"
        out += "};\n"
        return out

    hdr = "// MIDI 分段乐谱（自动生成：按一个键播完一段）\n#pragma once\n\n"
    hdr += fmt(seg_keys, "SCORE", "int")
    hdr += "\n" + fmt(seg_dur, "SEG_DUR_MS", "int")
    hdr += "\n" + fmt(seg_start, "SEG_NOTE_START", "int")
    hdr += "\n" + fmt(seg_count, "SEG_NOTE_COUNT", "int")
    hdr += "\n" + fmt(note_keys, "SEG_NOTES_KEY", "int")
    hdr += "\n" + fmt(note_offsets, "SEG_NOTES_OFFSET", "int")
    hdr += f"\n#define SCORE_LEN {len(seg_keys)}\n"
    hdr += f"#define SEG_NOTES_LEN {len(note_keys)}\n"
    with open("components/audio/score_data.h", 'w', encoding='utf-8') as f:
        f.write(hdr)
    print("已写入 components/audio/score_data.h")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "Love Story (1).mid")
