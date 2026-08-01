#!/usr/bin/env python3
"""
高质量钢琴采样生成器
- 物理建模：击弦噪音 + 分频衰减 + 不和谐泛音 + 琴弦共鸣
- 输出 16-bit 44100Hz 单声道 WAV → C 数组
每个音符总样本数:
  C4-G4 (6键): 2秒
  A4-E5 (4键): 1.2秒
总共 ≈ 1.3MB flash
"""

import wave
import math
import struct
import random
import os

SAMPLE_RATE = 44100
BITS = 16
MAX_VAL = 32767

# 10个音符
NOTES = [
    ("Do",   262, 2.0),    # C4
    ("Re",   294, 2.0),    # D4
    ("Mi",   330, 2.0),    # E4
    ("Fa",   349, 2.0),    # F4
    ("Sol",  392, 2.0),    # G4
    ("La",   440, 2.0),    # A4
    ("Si",   494, 1.2),    # B4
    ("HiDo", 523, 1.2),    # C5
    ("HiRe", 587, 1.2),    # D5
    ("HiMi", 659, 1.2),    # E5
]

# 每个谐波的衰减时长（秒）——越高频衰减越快，模拟真实钢琴
def harmonic_decay_time(h, duration):
    """谐波 h 的衰减时间常数（h=1 为基频）"""
    # 基频衰减最慢，高次谐波指数衰减
    return duration * (1.0 / (1.0 + 0.15 * h * h))

# 不和谐系数（钢琴弦刚性导致高次谐波略微升高）
INHARMONICITY = 0.00006


def gen_note(freq, duration_sec):
    """
    生成一个钢琴音符的采样数据
    返回 list of float (-1.0 ~ 1.0)
    """
    total_samples = int(SAMPLE_RATE * duration_sec)
    samples = [0.0] * total_samples
    random.seed(freq * 1000)  # 确定性随机，方便复现

    # ---- 1. 击弦噪音 (hammer noise) ----
    # 5ms 带通滤波噪声模拟琴槌击弦
    noise_len = int(0.005 * SAMPLE_RATE)  # 5ms
    for i in range(noise_len):
        if i >= total_samples:
            break
        # 包络：极快衰减
        t = i / SAMPLE_RATE
        env = math.exp(-t / 0.002)  # 2ms 衰减常数
        # 带通特性：freq 附近的高频成分
        noise = random.uniform(-1, 1)
        # 阻尼振荡模拟
        samples[i] += noise * env * 0.35

    # ---- 2. 谐波合成（25次谐波，各有独立衰减包络）----
    num_harmonics = 25
    for h in range(1, num_harmonics + 1):
        # 不和谐频率
        f_h = h * freq * math.sqrt(1.0 + INHARMONICITY * h * h)

        # 初始振幅（钢琴频谱：基频最强，高次衰减）
        if h == 1:
            amp = 1.0
        elif h == 2:
            amp = 0.55
        elif h == 3:
            amp = 0.35
        elif h == 4:
            amp = 0.22
        elif h == 5:
            amp = 0.15
        elif h == 6:
            amp = 0.10
        elif h <= 10:
            amp = 0.06 / (h - 5)
        else:
            amp = 0.02 / (h - 8)

        # 这个谐波的衰减时常
        decay_t = harmonic_decay_time(h, duration_sec)

        # 初始相位（略有随机）
        phase = random.uniform(0, 2 * math.pi)

        for i in range(total_samples):
            t = i / SAMPLE_RATE
            # 衰减包络：指数衰减
            env = math.exp(-t / decay_t)
            # 起音（所有谐波同步起音，~3ms）
            if t < 0.003:
                env *= 0.5 + 0.5 * (t / 0.003)  # 线性起音
            # 低次谐波的微小"膨胀"（琴弦共鸣效果）
            if h <= 3 and t > 0.05 and t < 0.3:
                bloom = 1.0 + 0.06 * math.sin((t - 0.05) / 0.25 * math.pi)
                env *= bloom

            samples[i] += amp * math.sin(2 * math.pi * f_h * t + phase) * env

    # ---- 3. 琴弦共鸣 (sympathetic resonance) ----
    # 非常微弱的额外共振，在高频区域
    for i in range(total_samples):
        t = i / SAMPLE_RATE
        if t < 0.02:
            continue
        # 几个高频共振
        res = 0
        for k in [3, 5, 8, 12]:
            f_r = freq * k * 1.002  # 略微偏移
            env_r = math.exp(-t / 0.15) * 0.012
            res += math.sin(2 * math.pi * f_r * t) * env_r
        samples[i] += res

    # ---- 4. 归一化 & 软限幅 ----
    max_abs = max(abs(s) for s in samples)
    if max_abs > 0:
        gain = 0.92 / max_abs  # 留余量防止削波
    else:
        gain = 1.0

    # 软削波
    for i in range(len(samples)):
        s = samples[i] * gain
        # 软削波（tanh）
        if s > 1.0:
            s = 1.0
        elif s < -1.0:
            s = -1.0
        samples[i] = s

    return samples


def samples_to_int16(samples):
    """float[-1,1] → int16"""
    return [max(-32768, min(32767, int(s * 32767))) for s in samples]


def write_wav(path, samples):
    """写入 WAV 文件"""
    int_samples = samples_to_int16(samples)
    with wave.open(path, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(struct.pack(f'<{len(int_samples)}h', *int_samples))
    print(f"  WAV → {path}  ({len(int_samples)} samples, {len(int_samples)/SAMPLE_RATE:.1f}s)")


def write_c_array(name, samples, h_file, c_file):
    """将采样写入 C 头文件声明和 .c 数组定义"""
    int_samples = samples_to_int16(samples)
    var_name = f"g_sample_{name.lower()}"

    # 头文件声明
    h_file.write(f"extern const int16_t {var_name}[];\n")
    h_file.write(f"extern const uint32_t {var_name}_len;\n\n")

    # .c 文件定义
    c_file.write(f"// {name} ({len(int_samples)} samples)\n")
    c_file.write(f"const int16_t {var_name}[] = {{\n")
    # 每行16个值
    for i in range(0, len(int_samples), 16):
        chunk = int_samples[i:i+16]
        c_file.write("    " + ", ".join(f"{v:6d}" for v in chunk) + ",\n")
    c_file.write(f"}};\n")
    c_file.write(f"const uint32_t {var_name}_len = {len(int_samples)};\n\n")
    return var_name, len(int_samples)


def main():
    out_dir = os.path.join(os.path.dirname(__file__), "..", "components", "audio")
    wav_dir = os.path.join(out_dir, "wav")
    os.makedirs(wav_dir, exist_ok=True)

    h_path = os.path.join(out_dir, "piano_samples.h")
    c_path = os.path.join(out_dir, "piano_samples.c")

    with open(h_path, 'w', encoding='utf-8') as hf, \
         open(c_path, 'w', encoding='utf-8') as cf:

        hf.write("// 钢琴采样数据 — 自动生成，请勿手动编辑\n")
        hf.write("#pragma once\n")
        hf.write("#include <stdint.h>\n\n")
        hf.write("// 采样率\n")
        hf.write(f"#define PIANO_SAMPLE_RATE {SAMPLE_RATE}\n\n")

        cf.write("// 钢琴采样数据 — 自动生成，请勿手动编辑\n")
        cf.write('#include "piano_samples.h"\n\n')

        total_bytes = 0
        for name, freq, duration in NOTES:
            print(f"生成 {name} ({freq}Hz, {duration}s)...")
            samples = gen_note(freq, duration)

            # 写 WAV
            wav_path = os.path.join(wav_dir, f"{name}.wav")
            write_wav(wav_path, samples)

            # 写 C 数组
            var, length = write_c_array(name, samples, hf, cf)
            total_bytes += length * 2
            print(f"  → {var}[{length}]")

        # 音符索引表
        hf.write("// 10个音符的查找表\n")
        hf.write("typedef struct {\n")
        hf.write("    const int16_t *data;\n")
        hf.write("    uint32_t length;\n")
        hf.write("    uint32_t freq;\n")
        hf.write("} piano_sample_t;\n\n")
        hf.write("extern const piano_sample_t g_piano_samples[10];\n")

        cf.write("// 音符查找表（索引0-9对应 Do~HiMi）\n")
        cf.write("const piano_sample_t g_piano_samples[10] = {\n")
        for name, freq, _ in NOTES:
            n = name.lower()
            cf.write(f"    {{ g_sample_{n}, g_sample_{n}_len, {freq} }},\n")
        cf.write("};\n")

        print(f"\n总大小: {total_bytes} 字节 ({total_bytes/1024:.1f} KB)")


if __name__ == "__main__":
    main()
