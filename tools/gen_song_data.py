#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 believer.json 重新生成固件 song_data_*.h（带 per-event durationMs）。

用法:
    python tools/gen_song_data.py <believer.json> <输出目录>

事件源: $.tracks.<track>.events[] = { tMs, keyId, laneColor, durationMs, source }
生成的 struct:
    song_event_t { uint32_t tMs; uint8_t keyId; uint16_t durationMs; }

打击/弦乐各生成一个头文件, 与 WEB-HW-ALIGN.md 对齐（颜色/时长以 Web 为权威）。
"""
import json
import sys
import os

TRACKS = ("percussion", "strings")


def emit_header(path, label, duration_ms, bpm, events):
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"/* Auto-generated from songs/believer.json · {label} · DO NOT EDIT */\n")
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n")
        f.write("\n")
        f.write("typedef struct {\n")
        f.write("  uint32_t tMs;\n")
        f.write("  uint8_t keyId;\n")
        f.write("  uint16_t durationMs;\n")
        f.write("} song_event_t;\n")
        f.write("\n")
        f.write(f"#define SONG_DURATION_MS {duration_ms}u\n")
        f.write(f"#define SONG_BPM {bpm}u\n")
        f.write(f"#define SONG_EVENT_COUNT {len(events)}u\n")
        f.write("\n")
        f.write("static const song_event_t SONG_EVENTS[SONG_EVENT_COUNT] = {\n")
        for e in events:
            dur = int(e.get("durationMs", 0))
            f.write("  { %du, %d, %du },\n" % (int(e["tMs"]), int(e["keyId"]), dur))
        f.write("};\n")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    json_path = sys.argv[1]
    out_dir = sys.argv[2]

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    duration_ms = int(data["durationMs"])
    bpm = int(data["bpm"])

    for track in TRACKS:
        events = data["tracks"][track]["events"]
        # 确保按 tMs 排序（原数据已排序，双保险）
        events = sorted(events, key=lambda e: int(e["tMs"]))
        out = os.path.join(out_dir, f"song_data_{'perc' if track == 'percussion' else 'str'}.h")
        emit_header(out, track, duration_ms, bpm, events)
        print(f"[ok] {out}: {len(events)} events")


if __name__ == "__main__":
    main()
