# SPDX-License-Identifier: GPL-3.0-or-later
"""Read Osmo lens tracks and their factory geometry without loading the movie."""

import json
import math
import struct
import subprocess
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path


def fields(data):
    # The DJI calibration packet layout is documented by PanoForge (MIT).
    def varint(offset):
        value = 0
        for shift in range(0, 70, 7):
            byte = data[offset]
            offset += 1
            value |= (byte & 127) << shift
            if byte < 128:
                return value, offset
        raise ValueError("Invalid metadata integer")

    result = {}
    i = 0
    while i < len(data):
        tag, i = varint(i)
        wire = tag & 7
        if wire == 0:
            value, i = varint(i)
        else:
            if wire == 2:
                size, i = varint(i)
            elif wire in (1, 5):
                size = 8 if wire == 1 else 4
            else:
                raise ValueError("Unsupported metadata wire format")
            if i + size > len(data):
                raise ValueError("Incomplete metadata field")
            value = data[i : i + size]
            i += size
        result.setdefault(tag >> 3, []).append(value)
    return result


def lenses_from_packet(packet, width):
    header = fields(packet)
    calibration = fields(fields(header[2][0])[6][0])
    lenses = []
    for group in sorted(calibration):
        for entry in calibration[group]:
            lens = fields(entry)
            if 21 not in lens:
                continue

            def numbers(k):
                value = lens[k][0]
                return list(struct.unpack("<" + "f" * (len(value) // 4), value))

            def first(k):
                return numbers(k)[0]

            scale = width / first(10)
            info = {k: first(i) * scale for i, k in enumerate(("fx", "fy", "cx", "cy"), 1)}
            info.update(
                width=width,
                height=first(11) * scale,
                dist=[first(i) for i in range(5, 9)],
                extrinsic_quat=numbers(21),
            )
            for i, k in [(22, "radial_lut_1"), (23, "radial_lut_2")]:
                if i in lens:
                    info[k] = [x * scale for x in numbers(i)]
            if (
                len(info["extrinsic_quat"]) != 4
                or not 0.8 < sum(x * x for x in info["extrinsic_quat"]) < 1.2
            ):
                return None
            if not all(
                math.isfinite(x) for v in info.values() for x in (v if isinstance(v, list) else [v])
            ):
                return None
            lenses.append(info)
            if len(lenses) == 2:
                return {"lenses": lenses}
    return None


def probe_json(path, args):
    result = subprocess.run(
        ["ffprobe", "-v", "error", *args, "-of", "json", str(path)], capture_output=True, timeout=45
    )
    if result.returncode:
        raise ValueError(result.stderr.decode(errors="replace")[-500:] or "Could not open recording")
    return json.loads(result.stdout)


@dataclass
class Clip:
    path: Path
    lenses: tuple
    width: int
    fps: Fraction
    frames: int
    calibration: dict | None
    color: dict


def open_clip(path):
    path = Path(path).resolve(strict=True)
    data = probe_json(path, ["-show_streams", "-show_format"])
    videos = [
        s
        for s in data["streams"]
        if s["codec_type"] == "video"
        and not s.get("disposition", {}).get("attached_pic")
        and s["width"] == s["height"]
    ]
    if len(videos) != 2 or videos[0]["width"] != videos[1]["width"]:
        raise ValueError("This editor needs an original dual-lens Osmo 360 .OSV recording.")
    first = videos[0]
    fps = Fraction(first["r_frame_rate"])
    if not 0 < fps <= 240 or Fraction(videos[1]["r_frame_rate"]) != fps:
        raise ValueError("The two lenses must have the same frame rate.")
    frames = int(first.get("nb_frames") or round(float(data["format"]["duration"]) * fps))
    packets = probe_json(
        path,
        [
            "-select_streams",
            "d",
            "-read_intervals",
            "%+1",
            "-show_packets",
            "-show_entries",
            "packet=stream_index,pos,size",
        ],
    ).get("packets", [])
    metadata = {s["index"] for s in data["streams"] if s.get("codec_tag_string") == "djmd"}
    seen = set()
    calibration = None
    with path.open("rb") as f:
        for packet in packets:
            index = packet["stream_index"]
            if index not in metadata or index in seen:
                continue
            seen.add(index)
            size = int(packet["size"])
            if not 0 < size <= 4_000_000 or "pos" not in packet:
                continue
            f.seek(int(packet["pos"]))
            try:
                calibration = lenses_from_packet(f.read(size), first["width"])
            except (KeyError, IndexError, TypeError, ValueError, struct.error, ZeroDivisionError):
                continue
            if calibration:
                break
    return Clip(
        path,
        tuple(v["index"] for v in videos),
        first["width"],
        fps,
        frames,
        calibration,
        {
            key: first[key]
            for key in ("color_primaries", "color_transfer", "color_space")
            if first.get(key) not in (None, "unknown", "reserved", "unspecified")
        },
    )
