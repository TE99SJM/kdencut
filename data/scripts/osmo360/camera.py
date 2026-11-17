# SPDX-License-Identifier: GPL-3.0-or-later
"""A virtual camera track. Frame numbers refer to the original OSV."""

import json
import math
import os
import tempfile
from bisect import bisect_right
from dataclasses import astuple, dataclass
from pathlib import Path


def wrap(angle):
    return (angle + 180) % 360 - 180


@dataclass(frozen=True)
class View:
    yaw: float = 0
    pitch: float = 0
    roll: float = 0
    fov: float = 90

    def __post_init__(self):
        limits = ((-180, 180), (-90, 90), (-180, 180), (25, 145))
        if any(not lo <= value <= hi for value, (lo, hi) in zip(astuple(self), limits)):
            raise ValueError("Camera angle or field of view is outside the supported range.")

    def ffmpeg(self, width, height):
        vertical = math.degrees(2 * math.atan(math.tan(math.radians(self.fov / 2)) * height / width))
        return dict(yaw=self.yaw, pitch=self.pitch, roll=self.roll, h_fov=self.fov, v_fov=vertical)


def interpolate(keys, frame, positions=None):
    positions = sorted(keys) if positions is None else positions
    i = bisect_right(positions, frame)
    if not i:
        return keys[positions[0]]
    start = positions[i - 1]
    if i == len(positions) or frame == start:
        return keys[start]
    end = positions[i]
    a, b = keys[start], keys[end]
    t = (frame - start) / (end - start)
    return View(
        wrap(a.yaw + t * wrap(b.yaw - a.yaw)),
        a.pitch + t * (b.pitch - a.pitch),
        wrap(a.roll + t * wrap(b.roll - a.roll)),
        a.fov + t * (b.fov - a.fov),
    )


def save_edit(path, clip, keys, mode):
    path = Path(path)
    try:
        source = os.path.relpath(clip.path, path.parent)
    except ValueError:  # different Windows drive
        source = str(clip.path)
    obj = dict(
        version=1,
        source=source,
        fps=str(clip.fps),
        frames=clip.frames,
        mode=mode,
        keys={i: astuple(v) for i, v in keys.items()},
    )
    with tempfile.NamedTemporaryFile("w", dir=path.parent, delete=False, encoding="utf8") as tmp:
        tmp_path = Path(tmp.name)
        try:
            json.dump(obj, tmp, indent=2)
            tmp.close()
            os.replace(tmp_path, path)
        finally:
            tmp_path.unlink(missing_ok=True)


def load_edit(path):
    from media import open_clip

    path = Path(path).resolve()
    if path.suffix.lower() != ".k360":
        return open_clip(path), {0: View()}, "Landscape"
    data = json.loads(path.read_text(encoding="utf8"))
    if data.get("version") != 1:
        raise ValueError("This 360 edit uses an unsupported version.")
    clip = open_clip(path.parent / data["source"])
    keys = {int(i): View(*v) for i, v in data["keys"].items()}
    if (
        not keys
        or min(keys) < 0
        or max(keys) >= clip.frames
        or str(clip.fps) != data["fps"]
        or clip.frames != data["frames"]
    ):
        raise ValueError("The source recording no longer matches the saved camera track.")
    if data["mode"] not in MODES:
        raise ValueError("Unsupported output shape.")
    return clip, keys, data["mode"]


MODES = {"Landscape": (16, 9), "Portrait": (9, 16), "Square": (1, 1), "360 panorama": (2, 1)}
