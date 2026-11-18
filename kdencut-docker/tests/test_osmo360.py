# SPDX-License-Identifier: GPL-3.0-or-later
"""Check keyframes and tagged MP4 renders."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ADDON = Path(__file__).resolve().parents[2] / "data/scripts/osmo360"
sys.path.insert(0, str(ADDON))
from camera import CameraTrack, RollKey, View, interpolate, load_edit, load_track, save_edit
from engine import Cancelled, Job, render
from media import open_clip


class CameraTests(unittest.TestCase):
    def test_crossing_the_back_seam(self):
        keys = {0: View(170, 0, 0, 80), 10: View(-170, 20, 0, 100)}
        self.assertEqual(interpolate(keys, 5), View(-180, 10, 0, 90))
        self.assertEqual(interpolate(keys, 999), keys[10])

    def test_pan_and_hold_roll_independently(self):
        aim = {0: View(yaw=170), 10: View(yaw=-170, fov=70)}
        track = CameraTrack(aim, {6: RollKey(18), 8: RollKey(-12)})
        # A later pan/zoom key must not reset a roll set on an earlier frame.
        aim[7] = View(yaw=-175, fov=75)
        combined = track.keyframes()
        for frame in range(13):
            view = interpolate(combined, frame)
            base = interpolate(aim, frame)
            self.assertAlmostEqual(view.yaw, base.yaw)
            self.assertAlmostEqual(view.fov, base.fov)
            self.assertEqual(view.pitch, 0)
            expected = 0 if frame < 6 else 18 if frame < 8 else -12
            self.assertEqual(view.roll, expected)
        self.assertEqual(track.frames(), {0, 6, 7, 8, 10})

    def test_legacy_roll_interpolation(self):
        keys = {3: View(roll=170), 10: View(yaw=30, roll=-170)}
        track = CameraTrack.from_legacy(keys)
        self.assertFalse(track.pan_only)
        for frame in range(13):
            self.assertEqual(interpolate(keys, frame), interpolate(track.keyframes(), frame))

    def test_dual_lens_export(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "two lenses.OSV"
            subprocess.run(
                [
                    "ffmpeg",
                    "-loglevel",
                    "error",
                    "-f",
                    "lavfi",
                    "-i",
                    "testsrc2=s=64x64:r=10",
                    "-f",
                    "lavfi",
                    "-i",
                    "smptebars=s=64x64:r=10",
                    "-f",
                    "lavfi",
                    "-i",
                    "sine=frequency=440",
                    "-map",
                    "0:v",
                    "-map",
                    "1:v",
                    "-map",
                    "2:a",
                    "-t",
                    "1",
                    "-c:v",
                    "libx264",
                    "-c:a",
                    "aac",
                    "-f",
                    "mp4",
                    str(source),
                ],
                check=True,
            )
            clip = open_clip(source)
            keys = {0: View(), 9: View(90, 5, 0, 70)}
            edit = root / "edit.k360"
            save_edit(edit, clip, keys, "Portrait")
            self.assertEqual(load_edit(edit)[1:], (keys, "Portrait"))
            track = CameraTrack({0: View(), 9: View(yaw=90)}, {6: RollKey(-15)})
            save_edit(edit, clip, track, "Portrait")
            self.assertEqual(load_track(edit)[1:], (track, "Portrait"))
            keys = track.keyframes()
            for name, size, sphere in [("portrait", (90, 160), False), ("panorama", (256, 128), True)]:
                target = root / (name + ".mp4")
                render(clip, keys, size, sphere, target, Job(), lambda n: None)
                info = json.loads(
                    subprocess.check_output(
                        [
                            "ffprobe",
                            "-v",
                            "error",
                            "-count_frames",
                            "-show_streams",
                            "-of",
                            "json",
                            str(target),
                        ]
                    )
                )
                video, audio = info["streams"]
                self.assertEqual((video["width"], video["height"]), size)
                self.assertEqual(int(video["nb_read_frames"]), 10)
                self.assertEqual(audio["codec_name"], "aac")
                spherical = any(
                    d.get("projection") == "equirectangular" for d in video.get("side_data_list", [])
                )
                self.assertEqual(spherical, sphere)
            original = target.read_bytes()
            stop = Job()
            stop.cancel()
            with self.assertRaises(Cancelled):
                render(clip, keys, size, sphere, target, stop, lambda n: None)
            self.assertEqual(original, target.read_bytes())


if __name__ == "__main__":
    unittest.main()
