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
from camera import View, interpolate, load_edit, save_edit
from engine import Cancelled, Job, render
from media import open_clip


class CameraTests(unittest.TestCase):
    def test_crossing_the_back_seam(self):
        keys = {0: View(170, 0, 0, 80), 10: View(-170, 20, 0, 100)}
        self.assertEqual(interpolate(keys, 5), View(-180, 10, 0, 90))
        self.assertEqual(interpolate(keys, 999), keys[10])

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
