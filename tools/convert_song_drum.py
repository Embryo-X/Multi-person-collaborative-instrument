#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
按 song.json 的 percussion 事件 midi 生成 GM 鼓音色 C 表（按 midi 音符索引）。

用法:
    python tools/convert_song_drum.py <song.json> <drums_dir> <out_dir> [song2.json drums_dir2 ...]

从所有歌的事件里收集 percussion 用到的 midi 并集 → 对应 drums_dir/*.mp3（note 命名，♭记法）
→ 转 PCM 并生成 gm_drum_samples.h/.c（每采样最长 0.5s，省 Flash）。
结构为 g_gm_drums[] = { midi, data, length } + count；固件按事件 midi 查表发声。
"""
import json, sys, os, subprocess, wave, struct

FFMPEG = "C:/Users/lenovo/AppData/Roaming/bilibili/ffmpeg/ffmpeg.exe"
NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
FLAT = {'C#':'Db','D#':'Eb','F#':'Gb','G#':'Ab','A#':'Bb'}
MAX_LEN = 44100 // 2   # 截短到 0.5s 省 Flash
FADE = 4410            # 100ms 淡出

def midi_filename(m):
    n = NAMES[m % 12]
    octv = m // 12 - 1
    return f"{FLAT.get(n, n)}{octv}.mp3"

def main():
    if len(sys.argv) < 4 or (len(sys.argv) - 2) % 2 != 0:
        print(__doc__); sys.exit(1)
    out_dir = sys.argv[-1]
    pairs = [(sys.argv[i], sys.argv[i+1]) for i in range(1, len(sys.argv)-1, 2)]

    # 收集所有歌 percussion 事件用到的 midi 并集（事件无 midi 时用 keymap 兜底）
    midis = []
    for song, _ in pairs:
        with open(song, encoding='utf-8') as f:
            data = json.load(f)
        perc = data['tracks']['percussion']
        kmap = {}
        for seg in perc.get('segments', []):
            for k, notes in seg.get('keymap', {}).items():
                kid = int(k)
                if kid not in kmap and notes:
                    kmap[kid] = int(notes[0])
        for e in perc['events']:
            m = int(e['midi']) if e.get('midi') is not None else kmap.get(int(e['keyId']), 0)
            if m and m not in midis:
                midis.append(m)
    print("并集 midi:", midis)

    # 1. ffmpeg 转 44100 单声道 16bit（从任一包找该 midi 的 note 采样）
    wav_dir = os.path.join(out_dir, "wav_gm")
    os.makedirs(wav_dir, exist_ok=True)
    wavs = []
    for m in midis:
        fn = midi_filename(m)
        src = None
        for _, drums_dir in pairs:
            cand = os.path.join(drums_dir, fn)
            if os.path.exists(cand):
                src = cand; break
        out = os.path.join(wav_dir, f"midi{m}.wav")
        if not src:
            print(f"[WARN] 缺采样 {fn} (midi {m})，跳过"); continue
        r = subprocess.run([FFMPEG, "-y", "-i", src, "-ar", "44100", "-ac", "1",
                            "-sample_fmt", "s16", out], capture_output=True, text=True)
        if r.returncode == 0:
            wavs.append((m, out, fn))
        else:
            print(f"[WARN] 转换失败 {fn}")

    # 2. 生成 C 表
    h_path = os.path.join(out_dir, "gm_drum_samples.h")
    c_path = os.path.join(out_dir, "gm_drum_samples.c")
    with open(h_path, 'w', encoding='utf-8') as hf, open(c_path, 'w', encoding='utf-8') as cf:
        hf.write("// GM 鼓音色（按 MIDI 音符索引）自动生成\n#pragma once\n#include <stdint.h>\n\n")
        cf.write("// GM 鼓音色（按 MIDI 音符索引）— 自动生成\n#include \"gm_drum_samples.h\"\n\n")
        names = []
        for m, wav, fn in wavs:
            with wave.open(wav, 'rb') as wf:
                raw = wf.readframes(wf.getnframes())
            samples = list(struct.unpack(f"<{len(raw)//2}h", raw))
            if len(samples) > MAX_LEN: samples = samples[:MAX_LEN]
            if len(samples) > FADE:
                for j in range(FADE):
                    g = 1.0 - j / FADE
                    samples[len(samples)-FADE+j] = int(samples[len(samples)-FADE+j] * g)
            mx = max(abs(v) for v in samples) if samples else 1
            if mx > 0:
                gain = 14000.0 / mx
                samples = [max(-32767, min(32767, int(v*g))) for v in samples]
            vn = f"g_gm_drum_{m}"
            names.append((m, vn))
            cf.write(f"const int16_t {vn}[] = {{\n")
            for j in range(0, len(samples), 16):
                cf.write("    " + ",".join(str(v) for v in samples[j:j+16]) + ",\n")
            cf.write("};\n")
            cf.write(f"const uint32_t {vn}_len = {len(samples)};\n\n")
            hf.write(f"extern const int16_t {vn}[];\nextern const uint32_t {vn}_len;\n\n")
            print(f"[ok] midi {m} ({fn}) → {len(samples)} samples")
        hf.write("typedef struct { uint8_t midi; const int16_t *data; uint32_t length; } gm_drum_t;\n")
        hf.write(f"extern const gm_drum_t g_gm_drums[{len(names)}];\n")
        hf.write(f"extern const int g_gm_drums_count;\n")
        cf.write("const gm_drum_t g_gm_drums[] = {\n")
        for m, vn in names:
            cf.write(f"    {{ {m}, {vn}, {vn}_len }},\n")
        cf.write("};\n")
        cf.write(f"const int g_gm_drums_count = {len(names)};\n")

if __name__ == "__main__":
    main()
