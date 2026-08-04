#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 believer.json 重新生成固件 song_data_*.h（带 per-event durationMs）。

用法:
    python tools/gen_song_data.py <song.json> <输出目录> [PREFIX]

事件源: $.tracks.<track>.events[] = { tMs, keyId, laneColor, durationMs, source, midi }
生成的 struct:
    song_event_t { uint32_t tMs; uint8_t keyId; uint16_t durationMs; uint8_t midi; }
PREFIX 给数组/宏加前缀（用于多首歌共存）。默认 PREFIX=SONG。
打击/弦乐各生成一个头文件。
"""
import json
import sys
import os

TRACKS = ("percussion", "strings")


def emit_header(path, label, duration_ms, bpm, events, prefix):
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"/* Auto-generated from songs/{label}.json · DO NOT EDIT */\n")
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n")
        f.write("\n")
        f.write("#ifndef SONG_EVENT_T_DEFINED\n")
        f.write("#define SONG_EVENT_T_DEFINED\n")
        f.write("typedef struct {\n")
        f.write("  uint32_t tMs;\n")
        f.write("  uint8_t keyId;\n")
        f.write("  uint16_t durationMs;\n")
        f.write("  uint8_t midi;\n")
        f.write("} song_event_t;\n")
        f.write("#endif\n")
        f.write("\n")
        f.write(f"#define {prefix}_DURATION_MS {duration_ms}u\n")
        f.write(f"#define {prefix}_BPM {bpm}u\n")
        f.write(f"#define {prefix}_EVENT_COUNT {len(events)}u\n")
        f.write("\n")
        f.write(f"static const song_event_t {prefix}_EVENTS[{prefix}_EVENT_COUNT] = {{\n")
        for e in events:
            dur = int(e.get("durationMs", 0))
            midi = int(e.get("midi", 0))
            f.write("  { %du, %d, %du, %d },\n" % (int(e["tMs"]), int(e["keyId"]), dur, midi))
        f.write("};\n")


def resolve_event_midis(track_obj):
    """事件若无 midi 字段，用 percussion/strings 的 keymap 兜底（每个 keyId 取第一个音作代表）"""
    kmap = {}
    for seg in track_obj.get("segments", []):
        for k, notes in seg.get("keymap", {}).items():
            kid = int(k)
            if kid not in kmap and notes:
                kmap[kid] = int(notes[0])
    for e in track_obj["events"]:
        if "midi" not in e or e["midi"] is None:
            e["midi"] = kmap.get(int(e["keyId"]), 0)
    return track_obj["events"]


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    json_path = sys.argv[1]
    out_dir = sys.argv[2]
    prefix = sys.argv[3].upper() if len(sys.argv) > 3 else "SONG"

    label = os.path.splitext(os.path.basename(json_path))[0]
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    duration_ms = int(data["durationMs"])
    bpm = int(data["bpm"])

    for track in TRACKS:
        events = resolve_event_midis(data["tracks"][track])
        # 确保按 tMs 排序（原数据已排序，双保险）
        events = sorted(events, key=lambda e: int(e["tMs"]))
        pfx = prefix + ("_STR" if track == "strings" else "")
        out = os.path.join(out_dir, f"song_data_{label}_{'perc' if track == 'percussion' else 'str'}.h")
        emit_header(out, label, duration_ms, bpm, events, pfx)
        print(f"[ok] {out}: {len(events)} events ({pfx})")


if __name__ == "__main__":
    main()
