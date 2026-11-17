# SPDX-License-Identifier: GPL-3.0-or-later
"""The preview and render paths use the same FFmpeg stitch and virtual camera."""

import os
import subprocess
import tempfile
import threading
from pathlib import Path

from camera import interpolate
from lensmaps import generate_remap_maps
from spherical import mark_spherical


class Cancelled(Exception):
    pass


class Job:
    """Use one Job per worker, allowing the UI to cancel a seek or export."""

    def __init__(self):
        self.cancelled = threading.Event()
        self.lock = threading.Lock()
        self.process = None

    def cancel(self):
        self.cancelled.set()
        with self.lock:
            if self.process and self.process.poll() is None:
                self.process.kill()

    def run(self, args, cwd, progress=None, data=None):
        with self.lock:
            if self.cancelled.is_set():
                raise Cancelled()
            self.process = subprocess.Popen(
                args,
                cwd=cwd,
                stdin=subprocess.PIPE if data is not None else subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
        proc = self.process
        try:
            if progress:
                # Keep stderr out of the progress pipe and drain it even when FFmpeg fails.
                errors = []
                drain = threading.Thread(target=lambda: errors.append(proc.stderr.read()), daemon=True)
                drain.start()
                for line in proc.stdout:
                    if line.startswith(b"frame="):
                        progress(int(line.split(b"=", 1)[1]))
                proc.wait()
                drain.join()
                out, err = b"", b"".join(errors)
            else:
                out, err = proc.communicate(data)
            if self.cancelled.is_set():
                raise Cancelled()
            if proc.returncode:
                raise RuntimeError(
                    err.decode(errors="replace")[-1500:]
                    or f"FFmpeg stopped (exit {proc.returncode}). Check available memory and disk space."
                )
            return out
        finally:
            if proc.poll() is None:
                proc.kill()
            proc.wait()
            proc.stdout.close()
            proc.stderr.close()


def base():
    return [
        "ffmpeg",
        "-hide_banner",
        "-loglevel",
        "error",
        "-nostdin",
        "-y",
        "-filter_complex_threads",
        "3",
    ]


def stitch(clip, width, directory, start=0, preview_rate=None):
    args = base() + ["-threads", "1", "-thread_type", "slice"]
    if start:
        args += ["-ss", f"{float(start / clip.fps):.8f}"]
    args += ["-i", str(clip.path)]
    a, b = (f"[0:{i}]" for i in clip.lenses)
    sample = f"fps={preview_rate}," if preview_rate else ""
    if clip.calibration:
        # Match decoded lens size to the destination. All geometry is expressed
        # in source pixels, including the factory seam-circle coordinates.
        lens_width = min(clip.width, max(512, width // 2))
        factor = lens_width / clip.width
        calibrated = {"lenses": []}
        for original in clip.calibration["lenses"]:
            lens = dict(original)
            for key in ("fx", "fy", "cx", "cy", "width", "height"):
                lens[key] *= factor
            for key in ("radial_lut_1", "radial_lut_2"):
                if key in lens:
                    lens[key] = [v * factor for v in lens[key]]
            calibrated["lenses"].append(lens)
        root = Path(directory) / str(width)
        if not (root / "blend_mask.pgm").exists():
            generate_remap_maps(calibrated, width, width // 2, str(root))
        for name in ("xmap0", "ymap0", "xmap1", "ymap1", "blend_mask"):
            args += ["-i", str(root / (name + ".pgm"))]
        prepare = f"{sample}scale={lens_width}:{lens_width}:flags=bicubic,format=gbrp16le"
        graph = (
            f"{a}{prepare}[a];[a][1:v][2:v]remap[l];"
            f"{b}{prepare}[b];[b][3:v][4:v]remap[r];"
            "[5:v]format=gbrp16le[m];[l][r][m]maskedmerge,setsar=1"
        )
    else:
        graph = (
            f"{a}{b}hstack,{sample}v360=dfisheye:e:ih_fov=190:iv_fov=190:yaw=90:"
            f"w={width}:h={width // 2}:interp=line,setsar=1"
        )
    return args, graph


def panorama_frame(clip, frame, directory, job):
    args, graph = stitch(clip, 1024, directory, frame)
    return job.run(
        args
        + [
            "-filter_complex",
            graph + "[v]",
            "-map",
            "[v]",
            "-frames:v",
            "1",
            "-c:v",
            "ppm",
            "-threads",
            "1",
            "-f",
            "image2pipe",
            "pipe:1",
        ],
        directory,
    )


def viewport(view, w, h):
    params = ":".join(f"{k}={v:.7f}" for k, v in view.ffmpeg(w, h).items())
    return f"v360@view=e:flat:w={w}:h={h}:interp=line:reset_rot=1:{params}"


def preview(panorama, view, w, h, directory, job):
    return job.run(
        base()
        + [
            "-f",
            "image2pipe",
            "-c:v",
            "ppm",
            "-i",
            "pipe:0",
            "-vf",
            viewport(view, w, h),
            "-frames:v",
            "1",
            "-c:v",
            "ppm",
            "-threads",
            "1",
            "-f",
            "image2pipe",
            "pipe:1",
        ],
        directory,
        data=panorama,
    )


def camera_commands(clip, keys, w, h, directory, start, count):
    first = interpolate(keys, start)
    previous = first.ffmpeg(w, h)
    file = Path(directory) / "camera.cmd"
    changed = False
    positions = sorted(keys)
    with file.open("w") as f:
        for frame in range(max(start + 1, positions[0] + 1), min(start + count, positions[-1] + 1)):
            current = interpolate(keys, frame, positions).ffmpeg(w, h)
            commands = [f"v360@view {k} {v:.7f}" for k, v in current.items() if abs(v - previous[k]) > 1e-7]
            if commands:
                # Halfway between adjacent timestamps avoids rounding to the next frame.
                t = float((frame - start - 0.5) / clip.fps)
                f.write(f"{t:.8f} " + ", ".join(commands) + ";\n")
                changed = True
            previous = current
    return ("sendcmd=f=camera.cmd," if changed else "") + viewport(first, w, h)


def render(clip, keys, size, panorama, output, job, progress, *, start=0, count=None):
    output = Path(output).resolve()
    if output == clip.path or output.suffix.lower() != ".mp4":
        raise ValueError("Choose a new MP4 output file.")
    w, h = size
    count = clip.frames - start if count is None else min(count, clip.frames - start)
    if min(w, h, count) <= 0 or (panorama and w != h * 2):
        raise ValueError("Invalid output dimensions or clip range")
    with tempfile.TemporaryDirectory(prefix=".osmo360-", dir=output.parent) as folder:
        stage = Path(folder) / "render.mp4"
        sw = w if panorama else min(2 * clip.width, max(1024, 2 * max(w, h)))
        args, graph = stitch(clip, sw, folder, start)
        if not panorama:
            graph += ",setpts=PTS-STARTPTS," + camera_commands(clip, keys, w, h, folder, start, count)
        args += [
            "-filter_complex",
            graph + "[v]",
            "-map",
            "[v]",
            "-map",
            "0:a?",
            "-map_metadata",
            "-1",
            "-r",
            str(clip.fps),
            "-fps_mode",
            "cfr",
            "-t",
            f"{float(count / clip.fps):.9f}",
            "-c:a",
            "copy",
        ]
        # HEVC preserves 10-bit precision for a panorama master. Flat edits favor compatibility.
        if panorama:
            args += [
                "-c:v",
                "libx265",
                "-pix_fmt",
                "yuv420p10le",
                "-tag:v",
                "hvc1",
                "-preset",
                "fast",
                "-x265-params",
                "log-level=error:pools=none:frame-threads=1:rc-lookahead=0:bframes=0",
                "-crf",
                "18",
            ]
        else:
            args += ["-c:v", "libx264", "-pix_fmt", "yuv420p", "-preset", "veryfast", "-crf", "18"]
        for k, flag in [
            ("color_primaries", "-color_primaries"),
            ("color_transfer", "-color_trc"),
            ("color_space", "-colorspace"),
        ]:
            if k in clip.color:
                args += [flag, clip.color[k]]
        args += ["-threads", "4", "-progress", "pipe:1", "-nostats", str(stage)]
        job.run(args, folder, progress=progress)
        if panorama:
            mark_spherical(stage)
        if job.cancelled.is_set():
            raise Cancelled()
        os.replace(stage, output)
    return output


def make_proxy(clip, directory, job, progress):
    args, graph = stitch(clip, 1024, directory, preview_rate=8)
    target = Path(directory) / "preview.mp4"
    job.run(
        args
        + [
            "-filter_complex",
            graph + "[v]",
            "-map",
            "[v]",
            "-an",
            "-c:v",
            "libx264",
            "-g",
            "1",
            "-preset",
            "ultrafast",
            "-pix_fmt",
            "yuv420p",
            "-threads",
            "3",
            "-progress",
            "pipe:1",
            str(target),
        ],
        directory,
        progress=progress,
    )
    return target


def proxy_frame(proxy, seconds, directory, job):
    return job.run(
        base()
        + [
            "-ss",
            f"{seconds:.7f}",
            "-i",
            str(proxy),
            "-frames:v",
            "1",
            "-c:v",
            "ppm",
            "-threads",
            "1",
            "-f",
            "image2pipe",
            "pipe:1",
        ],
        directory,
    )
