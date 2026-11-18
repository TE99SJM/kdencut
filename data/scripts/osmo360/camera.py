# SPDX-License-Identifier: GPL-3.0-or-later
"""A virtual camera track. Frame numbers refer to the original OSV."""

import json
import math
import os
import tempfile
from bisect import bisect_right
from dataclasses import astuple, dataclass, replace
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


@dataclass(frozen=True)
class RollKey:
    angle: float
    smooth: bool = False  # Older edits interpolated into each roll key.

    def __post_init__(self):
        if not -180 <= self.angle <= 180 or type(self.smooth) is not bool:
            raise ValueError("Invalid roll key.")


@dataclass
class CameraTrack:
    aim: dict[int, View]
    rolls: dict[int, RollKey]
    pan_only: bool = True

    @classmethod
    def from_legacy(cls, keys):
        return cls(
            {f: replace(v, roll=0) for f, v in keys.items()},
            {f: RollKey(v.roll, True) for f, v in keys.items()},
            pan_only=False,
        )

    def copy(self):
        return CameraTrack(self.aim.copy(), self.rolls.copy(), self.pan_only)

    def frames(self):
        return self.aim.keys() | self.rolls.keys()

    def keyframes(self):
        """Combine the independent channels for the existing preview and renderer."""
        roll_curve = {}
        previous = View()
        for frame, key in sorted(self.rolls.items()):
            if frame > 0 and not key.smooth:
                # One frame separates a held angle from its new value.
                roll_curve.setdefault(frame - 1, previous)
            previous = View(roll=key.angle)
            roll_curve[frame] = previous
        if not roll_curve:
            return self.aim.copy()
        aim_frames = sorted(self.aim)
        roll_frames = sorted(roll_curve)
        return {
            frame: replace(
                interpolate(self.aim, frame, aim_frames),
                roll=interpolate(roll_curve, frame, roll_frames).roll,
            )
            for frame in self.aim.keys() | roll_curve.keys()
        }


def save_edit(path, clip, keys, mode):
    path = Path(path)
    try:
        source = os.path.relpath(clip.path, path.parent)
    except ValueError:  # different Windows drive
        source = str(clip.path)
    track = keys if isinstance(keys, CameraTrack) else None
    poses = track.aim if track else keys
    obj = dict(
        version=2 if track else 1,
        source=source,
        fps=str(clip.fps),
        frames=clip.frames,
        mode=mode,
        keys={i: astuple(v) for i, v in poses.items()},
    )
    if track:
        obj.update(pan_only=track.pan_only, rolls={i: astuple(k) for i, k in track.rolls.items()})
    with tempfile.NamedTemporaryFile("w", dir=path.parent, delete=False, encoding="utf8") as tmp:
        tmp_path = Path(tmp.name)
        try:
            json.dump(obj, tmp, indent=2)
            tmp.close()
            os.replace(tmp_path, path)
        finally:
            tmp_path.unlink(missing_ok=True)


def load_track(path):
    from media import open_clip

    path = Path(path).resolve()
    if path.suffix.lower() != ".k360":
        return open_clip(path), CameraTrack({0: View()}, {}), "Landscape"
    data = json.loads(path.read_text(encoding="utf8"))
    if data.get("version") not in (1, 2):
        raise ValueError("This 360 edit uses an unsupported version.")
    clip = open_clip(path.parent / data["source"])
    keys = {int(i): View(*v) for i, v in data["keys"].items()}
    if data["version"] == 1:
        track = CameraTrack.from_legacy(keys)
    else:
        rolls = {int(i): RollKey(*v) for i, v in data["rolls"].items()}
        if type(data["pan_only"]) is not bool or any(v.roll != 0 for v in keys.values()):
            raise ValueError("Invalid camera track.")
        track = CameraTrack(keys, rolls, data["pan_only"])
    if (
        not keys
        or min(track.frames()) < 0
        or max(track.frames()) >= clip.frames
        or str(clip.fps) != data["fps"]
        or clip.frames != data["frames"]
    ):
        raise ValueError("The source recording no longer matches the saved camera track.")
    if data["mode"] not in MODES:
        raise ValueError("Unsupported output shape.")
    return clip, track, data["mode"]


def load_edit(path):
    clip, track, mode = load_track(path)
    return clip, track.keyframes(), mode


MODES = {"Landscape": (16, 9), "Portrait": (9, 16), "Square": (1, 1), "360 panorama": (2, 1)}
