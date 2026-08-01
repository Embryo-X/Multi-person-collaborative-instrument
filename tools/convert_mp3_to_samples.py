#!/usr/bin/env python3
"""
将 88钢琴按键音 MP3 → WAV → C 数组
使用真实钢琴采样替换之前的物理建模采样
"""
import subprocess
import wave
import struct
import os

FFMPEG = "C:/Users/lenovo/AppData/Roaming/bilibili/ffmpeg/ffmpeg.exe"
SRC_DIR = "88钢琴按键音/钢琴音色MP3"
# 目标 WAV: 44100Hz 16-bit 单声道
TARGET_RATE = 44100

# 10个音符: (C变量名, MP3文件关键词, 频率)
NOTES = [
    ("do",   "40-C  -小字1组", 262),   # C4
    ("re",   "42-D -小字1组",  294),   # D4
    ("mi",   "44-E  -小字1组", 330),   # E4
    ("fa",   "45-F   -小字1组",349),   # F4
    ("sol",  "47-G  -小字1组", 392),   # G4
    ("la",   "49-A  -小字1组", 440),   # A4
    ("si",   "51-B    -小字1组",494),  # B4
    ("hido", "52-C    -小字2组",523),  # C5
    ("hire", "54-D   -小字2组", 587),  # D5
    ("himi", "56-E   -小字2组", 659),  # E5
]

def main():
    out_dir = "components/audio"
    wav_dir = os.path.join(out_dir, "wav")
    os.makedirs(wav_dir, exist_ok=True)

    h_path = os.path.join(out_dir, "piano_samples.h")
    c_path = os.path.join(out_dir, "piano_samples.c")

    with open(h_path, 'w', encoding='utf-8') as hf, \
         open(c_path, 'w', encoding='utf-8') as cf:

        hf.write("// 真实钢琴采样数据 — 88键钢琴录音\n")
        hf.write("#pragma once\n")
        hf.write("#include <stdint.h>\n\n")
        hf.write(f"#define PIANO_SAMPLE_RATE {TARGET_RATE}\n\n")

        cf.write("// 真实钢琴采样数据 — 88键钢琴录音\n")
        cf.write('#include "piano_samples.h"\n\n')

        total_size = 0

        for var_name, keyword, freq in NOTES:
            # 找对应的 MP3 文件
            mp3_files = []
            for f in os.listdir(SRC_DIR):
                if keyword in f and f.endswith('.mp3'):
                    mp3_files.append(f)

            if not mp3_files:
                print(f"❌ 找不到: {keyword}")
                continue

            mp3_path = os.path.join(SRC_DIR, mp3_files[0])
            wav_path = os.path.join(wav_dir, f"{var_name}.wav")

            # ffmpeg 转换 MP3 → WAV
            print(f"转换: {mp3_files[0]} → {var_name}.wav ...", end=" ")
            result = subprocess.run([
                FFMPEG, "-y", "-i", mp3_path,
                "-ar", str(TARGET_RATE), "-ac", "1",
                "-sample_fmt", "s16", wav_path
            ], capture_output=True, text=True)

            if result.returncode != 0:
                print(f"❌ ffmpeg 失败: {result.stderr[:200]}")
                continue

            # 读取 WAV
            with wave.open(wav_path, 'rb') as wf:
                nchannels = wf.getnchannels()
                sampwidth = wf.getsampwidth()
                framerate = wf.getframerate()
                nframes = wf.getnframes()
                raw = wf.readframes(nframes)

            # 转为 int16 列表
            fmt = f"<{nframes}h"
            samples = list(struct.unpack(fmt, raw))

            # 限制最大长度（2秒）
            max_samples = TARGET_RATE * 2
            if len(samples) > max_samples:
                samples = samples[:max_samples]

            # 尾部淡出 250ms：消除采样硬切断的爆音
            fade_samples = int(TARGET_RATE * 0.25)
            if len(samples) > fade_samples:
                for j in range(fade_samples):
                    gain = 1.0 - (j / fade_samples)
                    idx = len(samples) - fade_samples + j
                    samples[idx] = int(samples[idx] * gain)

            # 整体降增益 15%：给功放留 headroom，减少破音
            samples = [int(v * 0.85) for v in samples]

            # 写 C 数组
            cf.write(f"// {mp3_files[0]} ({len(samples)} samples, {framerate}Hz)\n")
            cf.write(f"const int16_t g_sample_{var_name}[] = {{\n")
            for i in range(0, len(samples), 16):
                chunk = samples[i:i+16]
                cf.write("    " + ", ".join(f"{v:6d}" for v in chunk) + ",\n")
            cf.write(f"}};\n")
            cf.write(f"const uint32_t g_sample_{var_name}_len = {len(samples)};\n\n")

            # 写头文件
            hf.write(f"extern const int16_t g_sample_{var_name}[];\n")
            hf.write(f"extern const uint32_t g_sample_{var_name}_len;\n\n")

            total_size += len(samples) * 2
            print(f"✓ {len(samples)} samples, {len(samples)/TARGET_RATE:.1f}s")

        # 查找表
        hf.write("typedef struct {\n")
        hf.write("    const int16_t *data;\n")
        hf.write("    uint32_t length;\n")
        hf.write("    uint32_t freq;\n")
        hf.write("} piano_sample_t;\n\n")
        hf.write("extern const piano_sample_t g_piano_samples[10];\n")

        cf.write("// 音符查找表（索引0-9对应 Do~HiMi）\n")
        cf.write("const piano_sample_t g_piano_samples[10] = {\n")
        for var_name, _, freq in NOTES:
            cf.write(f"    {{ g_sample_{var_name}, g_sample_{var_name}_len, {freq} }},\n")
        cf.write("};\n")

        print(f"\n✅ 完成！总大小: {total_size:,} 字节 ({total_size/1024:.1f} KB)")

if __name__ == "__main__":
    main()
