#!/usr/bin/env python3
"""Believer GM 鼓音色 → C 数组（9个，keyId 0-8）
keyId → MIDI → drums/*.mp3
0:36 C2低鼓  1:38 D2军鼓  2:42 Gb2闭镲  3:46 Bb2开镲
4:49 Db3镲片  5:51 Eb3叮叮  6:45 A2桶鼓  7:37 Db2侧击  8:39 Eb2拍手
"""
import subprocess, wave, struct, os

FFMPEG = "C:/Users/lenovo/AppData/Roaming/bilibili/ffmpeg/ffmpeg.exe"
SAMPLES = "believer-hw-handoff/believer-hw-handoff/samples/drums"
FILES = ["C2.mp3","D2.mp3","Gb2.mp3","Bb2.mp3","Db3.mp3","Eb3.mp3","A2.mp3","Db2.mp3","Eb2.mp3"]
OUT_DIR = "components/audio"
WAV_DIR = os.path.join(OUT_DIR, "wav_gm")
os.makedirs(WAV_DIR, exist_ok=True)

h_path = os.path.join(OUT_DIR, "gm_drum_samples.h")
c_path = os.path.join(OUT_DIR, "gm_drum_samples.c")

# 1. ffmpeg 转 44100 单声道 16bit
for i, f in enumerate(FILES):
    src = os.path.join(SAMPLES, f)
    out = os.path.join(WAV_DIR, f"{i}.wav")
    r = subprocess.run([FFMPEG, "-y", "-i", src, "-ar", "44100", "-ac", "1", "-sample_fmt", "s16", out],
                       capture_output=True, text=True)
    print(f"{f} → {out} {'OK' if r.returncode==0 else 'FAIL'}")

# 2. 生成 C 数组（截短 + 淡出）
MAX_LEN = 44100  # 最长1秒
FADE = 4410      # 100ms淡出
with open(h_path, 'w', encoding='utf-8') as hf, open(c_path, 'w', encoding='utf-8') as cf:
    hf.write("// Believer GM 鼓音色（keyId 0-8）自动生成\n#pragma once\n#include <stdint.h>\n\n")
    cf.write("// Believer GM 鼓音色 — 自动生成\n#include \"gm_drum_samples.h\"\n\n")
    total = 0
    for i in range(9):
        wav = os.path.join(WAV_DIR, f"{i}.wav")
        with wave.open(wav, 'rb') as wf:
            nframes = wf.getnframes()
            raw = wf.readframes(nframes)
        samples = list(struct.unpack(f"<{nframes}h", raw))
        if len(samples) > MAX_LEN: samples = samples[:MAX_LEN]
        if len(samples) > FADE:
            for j in range(FADE):
                g = 1.0 - j / FADE
                samples[len(samples)-FADE+j] = int(samples[len(samples)-FADE+j] * g)
        # 归一化放大到峰值 14000（GM 采样原始音量低，听不到）
        mx = max(abs(v) for v in samples) if samples else 1
        if mx > 0:
            gain = 14000.0 / mx
            samples = [max(-32767, min(32767, int(v * gain))) for v in samples]
        total += len(samples) * 2
        vn = f"g_gm_drum_{i}"
        cf.write(f"const int16_t {vn}[] = {{\n")
        for j in range(0, len(samples), 16):
            cf.write("    " + ",".join(str(v) for v in samples[j:j+16]) + ",\n")
        cf.write("};\n")
        cf.write(f"const uint32_t {vn}_len = {len(samples)};\n\n")
        hf.write(f"extern const int16_t {vn}[];\nextern const uint32_t {vn}_len;\n\n")
    hf.write("typedef struct { const int16_t *data; uint32_t length; } gm_drum_t;\n")
    hf.write("extern const gm_drum_t g_gm_drums[9];\n")
    cf.write("const gm_drum_t g_gm_drums[9] = {\n")
    for i in range(9):
        cf.write(f"    {{ g_gm_drum_{i}, g_gm_drum_{i}_len }},\n")
    cf.write("};\n")
    print(f"总大小: {total/1024:.1f} KB")
