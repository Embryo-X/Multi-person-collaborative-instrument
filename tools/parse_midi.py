#!/usr/bin/env python3
"""解析 MIDI 文件基本信息：音符范围、数量、时长"""
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
    assert data[:4] == b'MThd', "not MIDI"
    fmt, ntrks, div = struct.unpack('>HHH', data[8:14])
    print(f"格式:{fmt} 轨道数:{ntrks} 分辨率:{div} ticks/拍")

    notes = []   # (note, tick)
    i = 14
    for trk in range(ntrks):
        assert data[i:i+4] == b'MTrk', f"track {trk} bad"
        length = struct.unpack('>I', data[i+4:i+8])[0]
        end = i + 8 + length
        i += 8
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
            if msg == 0x90:  # note on
                note, vel = data[i], data[i+1]; i += 2
                if vel > 0:
                    notes.append((t, note))
            elif msg == 0x80:  # note off
                i += 2
            elif status in (0xF0, 0xF7):
                length_v, i = read_varint(data, i); i += length_v
            elif status == 0xFF:
                i += 1
                length_v, i = read_varint(data, i); i += length_v
            else:
                # 通道消息：程序/触后等
                if msg in (0xC0, 0xD0): i += 1
                else: i += 2
        i = end

    if not notes:
        print("无音符")
        return
    notes.sort()
    pitches = sorted(set(n for _, n in notes))
    # 时间换算：假设 tempo 120BPM，tick = 1/div 拍
    dur_sec = notes[-1][0] / div * (60/120)
    print(f"音符总数:{len(notes)}  不同音高:{len(pitches)}")
    print(f"音高范围: {min(pitches)} ({pitch_name(min(pitches))}) ~ {max(pitches)} ({pitch_name(max(pitches))})")
    print(f"总时长: {dur_sec:.1f} 秒 (120BPM估算)")
    # 音高分布
    from collections import Counter
    c = Counter(n for _, n in notes)
    print("前20个常见音高:")
    for n, cnt in c.most_common(20):
        print(f"  {n} {pitch_name(n)}: {cnt}")

def pitch_name(n):
    names = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
    return f"{names[n%12]}{n//12-1}"

if __name__ == "__main__":
    parse(sys.argv[1] if len(sys.argv) > 1 else "Genshin_Impact_罪人舞步旋.mid")
