#!/usr/bin/env python3
"""架子鼓音频 → C 数组 (9个, 1-8.wav + 9.mp3 → 16bit 44100 单声道)"""
import subprocess, wave, struct, os

FFMPEG = "C:/Users/lenovo/AppData/Roaming/bilibili/ffmpeg/ffmpeg.exe"
SRC = ["1.wav","2.wav","3.wav","4.wav","5.wav","6.wav","7.wav","8.wav","9.mp3"]
OUT_DIR = "components/audio"
WAV_DIR = os.path.join(OUT_DIR, "wav_drum")
os.makedirs(WAV_DIR, exist_ok=True)

h_path = os.path.join(OUT_DIR, "drum_samples.h")
c_path = os.path.join(OUT_DIR, "drum_samples.c")

# 1. ffmpeg 转 44100 单声道 16bit
for i, f in enumerate(SRC, 1):
    out = os.path.join(WAV_DIR, f"{i}.wav")
    r = subprocess.run([FFMPEG, "-y", "-i", f, "-ar", "44100", "-ac", "1", "-sample_fmt", "s16", out],
                       capture_output=True, text=True)
    print(f"{f} → {out} {'OK' if r.returncode==0 else 'FAIL'}")

# 2. 生成 C 数组
with open(h_path, 'w', encoding='utf-8') as hf, open(c_path, 'w', encoding='utf-8') as cf:
    hf.write("// 架子鼓采样数据 — 自动生成\n#pragma once\n#include <stdint.h>\n#define DRUM_SAMPLE_RATE 44100\n\n")
    cf.write("// 架子鼓采样数据 — 自动生成\n#include \"drum_samples.h\"\n\n")

    MAX_LEN = 44100 * 15 // 10      # 最长 1.5 秒
    FADE_LEN = 44100 // 10          # 尾部淡出 100ms

    total = 0
    for i in range(1, 10):
        wav = os.path.join(WAV_DIR, f"{i}.wav")
        with wave.open(wav, 'rb') as wf:
            nframes = wf.getnframes()
            raw = wf.readframes(nframes)
        samples = list(struct.unpack(f"<{nframes}h", raw))

        # 截短到最长 1.5s
        if len(samples) > MAX_LEN:
            samples = samples[:MAX_LEN]
        # 尾部淡出 100ms 防爆音
        if len(samples) > FADE_LEN:
            for j in range(FADE_LEN):
                g = 1.0 - j / FADE_LEN
                samples[len(samples) - FADE_LEN + j] = int(samples[len(samples) - FADE_LEN + j] * g)
        # 原降增益 20% 防破音；用户要求鼓更响，改为全幅（混音端 ±12000 限幅兜底）
        samples = [int(v) for v in samples]
        total += len(samples) * 2

        vn = f"g_drum_{i}"
        cf.write(f"const int16_t {vn}[] = {{\n")
        for j in range(0, len(samples), 16):
            cf.write("    " + ", ".join(f"{v:6d}" for v in samples[j:j+16]) + ",\n")
        cf.write("};\n")
        cf.write(f"const uint32_t {vn}_len = {len(samples)};\n\n")
        hf.write(f"extern const int16_t {vn}[];\nextern const uint32_t {vn}_len;\n\n")

    # 查找表（索引0-8 对应 key 1-9）
    hf.write("typedef struct {\n    const int16_t *data;\n    uint32_t length;\n} drum_sample_t;\n\n")
    hf.write("extern const drum_sample_t g_drum_samples[9];\n")
    cf.write("const drum_sample_t g_drum_samples[9] = {\n")
    for i in range(1, 10):
        cf.write(f"    {{ g_drum_{i}, g_drum_{i}_len }},\n")
    cf.write("};\n")

    print(f"总大小: {total/1024:.1f} KB")
