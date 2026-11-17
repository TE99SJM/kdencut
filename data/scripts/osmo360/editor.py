#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Osmo 360 clip editor, shared by KdenCut imports and stock Kdenlive Media Jobs."""

import argparse
import json
import math
import queue
import sys
import tempfile
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from camera import MODES, View, interpolate, load_edit, save_edit, wrap
from engine import (
    Cancelled,
    Job,
    make_proxy,
    panorama_frame,
    preview,
    proxy_frame,
    render,
)


class Editor:
    def __init__(self, root, source, output, folder):
        self.root = root
        self.clip, self.keys, mode = load_edit(source)
        self.output = output
        self.edit_path = source if source.suffix.lower() == ".k360" else None
        self.folder = Path(folder)
        self.thumb_dir = self.folder / "stills"
        self.thumb_dir.mkdir()
        self.proxy_dir = self.folder / "playback"
        self.proxy_dir.mkdir()
        self.result = None
        self.events = queue.Queue()
        self.frame = 0
        self.view = interpolate(self.keys, 0)
        self.pan = None
        self.cached = (None, None)
        self.thumb_job = self.background = None
        self.pending = None
        self.proxy = None
        self.play = False
        self.closing = self.dirty = self.setting = False
        self.history = []
        self.threads = []
        self.mode = tk.StringVar(value=mode)
        self.quality = tk.StringVar(value="Standard")
        self.status = tk.StringVar(value="Reading both lenses…")
        style = ttk.Style(root)
        style.theme_use("clam")
        style.configure(".", background="#20252a", foreground="#edf1f3", font=("TkDefaultFont", 10))
        style.configure("TButton", background="#363e45", borderwidth=0, padding=(6, 7))
        style.map("TButton", background=[("active", "#48535b"), ("disabled", "#2a3035")])
        style.configure("Primary.TButton", background="#78ded4", foreground="#102826")
        style.map("Primary.TButton", background=[("disabled", "#354a4b"), ("active", "#95f0e7")])
        style.configure("TScale", background="#78ded4", troughcolor="#424b52", slidersize=18)
        style.map(
            "TCombobox", fieldbackground=[("readonly", "#363e45")], foreground=[("readonly", "#edf1f3")]
        )
        root.configure(bg="#20252a")
        root.title("Osmo 360 Editor — " + self.clip.path.name)
        sw, sh = root.winfo_screenwidth(), root.winfo_screenheight()
        root.geometry(f"{min(840, sw - 12)}x{min(680, sh - 45)}")
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)
        self.canvas = tk.Canvas(root, bg="#15191c", highlightthickness=0, width=320, height=100)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        self.canvas.bind("<Configure>", lambda e: self.request())
        self.canvas.bind("<ButtonPress-1>", self.press)
        self.canvas.bind("<B1-Motion>", self.drag)
        self.canvas.bind("<ButtonRelease-1>", self.release)
        self.canvas.bind("<MouseWheel>", lambda e: self.zoom(-3 if e.delta > 0 else 3))
        self.canvas.bind("<Button-4>", lambda e: self.zoom(-3))
        self.canvas.bind("<Button-5>", lambda e: self.zoom(3))
        panel = ttk.Frame(root, padding=6)
        panel.grid(row=1, column=0, sticky="ew")
        panel.columnconfigure(0, weight=1)
        self.position = tk.DoubleVar()
        self.seekbar = ttk.Scale(
            panel, from_=0, to=self.clip.frames - 1, variable=self.position, command=self.seek
        )
        self.seekbar.grid(row=0, column=0, sticky="ew")
        self.position_label = ttk.Label(panel)
        self.position_label.grid(row=0, column=1, padx=3)

        def row(n):
            obj = ttk.Frame(panel)
            obj.grid(row=n, column=0, columnspan=2, sticky="ew", pady=2)
            return obj

        transport = row(1)
        self.play_button = ttk.Button(transport, text="Play", width=6, command=self.play_pause)
        tools = [self.play_button]
        for name, action in [
            ("‹ Key", lambda: self.jump(-1)),
            ("Key ›", lambda: self.jump(1)),
            ("Set key", self.set_key),
            ("Delete key", self.delete_key),
            ("Undo", self.undo),
        ]:
            tools.append(ttk.Button(transport, text=name, width=len(name), command=action))

        def arrange(event):
            columns = 3 if event.width < 480 else 6
            for i, button in enumerate(tools):
                button.grid(row=i // columns, column=i % columns, padx=2, pady=1, sticky="ew")
                transport.columnconfigure(i, weight=int(i < columns))

        transport.bind("<Configure>", arrange)
        self.angle = {}
        for i, (name, low, high) in enumerate([("FOV", 25, 145), ("Roll", -180, 180)]):
            line = row(2 + i)
            ttk.Label(line, text=name, width=5).pack(side="left")
            var = tk.DoubleVar()
            self.angle[name] = var
            scale = ttk.Scale(line, from_=low, to=high, variable=var, command=self.sliders)
            scale.pack(side="left", fill="x", expand=True)
            scale.bind("<ButtonRelease-1>", lambda e: self.set_key())
            scale.bind("<KeyRelease>", lambda e: self.set_key())
        options = row(4)
        ttk.Label(options, text="Output").pack(side="left")
        combo = ttk.Combobox(
            options, textvariable=self.mode, values=list(MODES), state="readonly", width=13
        )
        combo.pack(side="left", padx=4)
        combo.bind("<<ComboboxSelected>>", self.shape_changed)
        ttk.Combobox(
            options, textvariable=self.quality, values=["Standard", "High"], state="readonly", width=9
        ).pack(side="left")
        buttons = row(5)
        self.export_button = ttk.Button(
            buttons, text="Render and import" if output else "Export MP4", command=self.export
        )
        self.export_button.configure(style="Primary.TButton")
        self.export_button.pack(side="left")
        ttk.Button(buttons, text="Save edit…", command=self.save).pack(side="left", padx=3)
        self.stop = ttk.Button(buttons, text="Cancel job", command=self.stop_job, state="disabled")
        self.stop.pack(side="right")
        ttk.Label(panel, textvariable=self.status, wraplength=max(280, min(sw - 25, 700))).grid(
            row=6, column=0, columnspan=2, sticky="w"
        )
        root.protocol("WM_DELETE_WINDOW", self.close)
        self.refresh()
        root.after(25, self.tick)

    def thread(self, fn):
        self.threads = [t for t in self.threads if t.is_alive()]
        t = threading.Thread(target=fn, daemon=True)
        self.threads.append(t)
        t.start()

    def refresh(self):
        self.setting = True
        self.position.set(self.frame)
        self.angle["FOV"].set(self.view.fov)
        self.angle["Roll"].set(self.view.roll)
        self.position_label.config(
            text=f"{float(self.frame / self.clip.fps):.1f}s  " + ("◆" if self.frame in self.keys else "")
        )
        self.setting = False
        self.request()

    def dimensions(self):
        a, b = MODES[self.mode.get()]
        w, h = max(80, self.canvas.winfo_width()), max(60, self.canvas.winfo_height())
        scale = min(w / a, h / b, 720 / max(a, b))
        return max(2, 2 * int(a * scale / 2)), max(2, 2 * int(b * scale / 2))

    def request(self):
        if self.closing:
            return
        self.pending = (self.frame, self.view, self.dimensions(), self.mode.get(), self.proxy)

    def start_thumb(self):
        spec = self.pending
        self.pending = None
        frame, view, (w, h), mode, proxy = spec
        self.thumb_job = job = Job()
        cached_frame, cached_image = self.cached

        def work():
            try:
                img = cached_image
                if frame != cached_frame:
                    if proxy:
                        t = min(
                            float(frame / self.clip.fps),
                            max(0, (math.ceil(self.clip.frames * 8 / self.clip.fps) - 1) / 8),
                        )
                        img = proxy_frame(proxy, t, self.thumb_dir, job)
                    if not proxy or not img:
                        img = panorama_frame(self.clip, frame, self.thumb_dir, job)
                if mode == "360 panorama":
                    from io import BytesIO

                    from PIL import Image

                    target = BytesIO()
                    with Image.open(BytesIO(img)) as im:
                        im.resize((w, h)).save(target, "PPM")
                    data = target.getvalue()
                else:
                    data = preview(img, view, w, h, self.thumb_dir, job)
                self.events.put(("image", spec, img, data))
            except Exception as exc:
                self.events.put(("thumb-error", str(exc)))

        self.thread(work)

    def seek(self, value):
        if not self.setting:
            self.play = False
            self.move_to(round(float(value)))

    def move_to(self, n):
        self.frame = max(0, min(self.clip.frames - 1, n))
        self.view = interpolate(self.keys, self.frame)
        self.refresh()

    def jump(self, direction):
        after = [f for f in sorted(self.keys) if (f - self.frame) * direction > 0]
        if after:
            self.play = False
            self.move_to(after[0] if direction > 0 else after[-1])

    def press(self, e):
        self.play = False
        self.pan = (e.x, e.y, self.view)

    def drag(self, e):
        if self.pan and self.mode.get() != "360 panorama":
            x, y, v = self.pan
            scale = v.fov / max(1, self.dimensions()[0])
            self.view = View(
                wrap(v.yaw - (e.x - x) * scale),
                max(-90, min(90, v.pitch + (e.y - y) * scale)),
                v.roll,
                v.fov,
            )
            self.refresh()

    def release(self, e):
        if self.pan and self.pan[2] != self.view:
            self.set_key()
        self.pan = None

    def zoom(self, delta):
        self.play = False
        self.view = View(
            self.view.yaw, self.view.pitch, self.view.roll, max(25, min(145, self.view.fov + delta))
        )
        self.set_key()

    def sliders(self, value):
        if not self.setting:
            self.play = False
            self.view = View(
                self.view.yaw, self.view.pitch, self.angle["Roll"].get(), self.angle["FOV"].get()
            )
            self.request()

    def snapshot(self):
        self.history = (self.history + [self.keys.copy()])[-50:]
        self.dirty = True

    def set_key(self):
        if self.keys.get(self.frame) != self.view:
            self.snapshot()
            self.keys[self.frame] = self.view
        self.refresh()

    def delete_key(self):
        if len(self.keys) > 1 and self.frame in self.keys:
            self.snapshot()
            del self.keys[self.frame]
            self.move_to(self.frame)

    def undo(self):
        if self.history:
            self.keys = self.history.pop()
            self.dirty = True
            self.move_to(self.frame)

    def shape_changed(self, event):
        self.dirty = True
        self.request()

    def save(self):
        target = filedialog.asksaveasfilename(
            defaultextension=".k360",
            filetypes=[("Editable camera track", "*.k360")],
            initialdir=self.edit_path.parent if self.edit_path else self.clip.path.parent,
            initialfile=self.edit_path.name if self.edit_path else self.clip.path.stem + ".k360",
        )
        if target:
            try:
                save_edit(target, self.clip, self.keys, self.mode.get())
                self.edit_path = Path(target)
                self.dirty = False
                self.status.set("Saved the camera track.")
            except OSError as exc:
                messagebox.showerror("Save edit", str(exc))
        return bool(target) and not self.dirty

    def background_job(self, kind, fn):
        self.background = job = Job()
        self.export_button.config(state="disabled")
        self.stop.config(state="normal")

        def work():
            try:
                result = fn(job, lambda n: self.events.put(("progress", kind, n)))
                self.events.put(("done", kind, result))
            except Exception as exc:
                self.events.put(("failed", kind, exc))

        self.thread(work)

    def stop_job(self):
        if self.background:
            self.background.cancel()

    def play_pause(self):
        if self.background:
            return
        if not self.proxy:
            self.status.set("Preparing the 8 fps playback preview…")
            self.background_job("proxy", lambda j, p: make_proxy(self.clip, self.proxy_dir, j, p))
        elif self.play:
            self.play = False
        else:
            self.play = True
            self.start_time = time.monotonic()
            self.start_frame = self.frame if self.frame < self.clip.frames - 1 else 0

    def export(self):
        self.play = False
        mode = self.mode.get()
        target = self.output
        if not target:
            target = filedialog.asksaveasfilename(
                defaultextension=".mp4",
                filetypes=[("MP4 video", "*.mp4")],
                initialdir=self.clip.path.parent,
                initialfile=self.clip.path.stem + "-" + mode.split()[0] + ".mp4",
            )
        if not target:
            return
        high = self.quality.get() == "High"
        a, b = MODES[mode]
        if mode == "360 panorama":
            w = min(self.clip.width * 2, 7680 if high else 3840)
            size = (w, w // 2)
        else:
            longest = (2160 if high else 1080) if a == b else (3840 if high else 1920)
            size = tuple(2 * round(longest * d / max(a, b) / 2) for d in (a, b))
        keys = self.keys.copy()
        self.status.set("Rendering from the original lenses…")
        self.background_job(
            "render", lambda j, p: render(self.clip, keys, size, mode == "360 panorama", target, j, p)
        )

    def close(self):
        if self.closing:
            return
        if self.background and not messagebox.askyesno(
            "Cancel job", "Cancel the current job and close the editor?"
        ):
            return
        if self.dirty:
            save = messagebox.askyesnocancel(
                "Camera track", "Save the editable camera track before closing?"
            )
            if save is None or (save and not self.save()):
                return
        self.shutdown()

    def shutdown(self):
        self.closing = True
        self.stop_job()
        if self.thumb_job:
            self.thumb_job.cancel()
        if any(t.is_alive() for t in self.threads):
            self.root.after(60, self.shutdown)
        else:
            self.root.destroy()

    def tick(self):
        if self.closing:
            return
        while not self.events.empty():
            msg = self.events.get()
            if msg[0] == "image":
                _, spec, img, data = msg
                self.cached = (spec[0], img)
                self.thumb_job = None
                if not self.pending or spec[0] == self.frame:
                    self.photo = tk.PhotoImage(data=data, format="PPM")
                    self.canvas.delete("all")
                    self.canvas.create_image(
                        self.canvas.winfo_width() / 2, self.canvas.winfo_height() / 2, image=self.photo
                    )
                if not self.background and self.status.get() == "Reading both lenses…":
                    self.status.set(
                        ("Factory lens calibration" if self.clip.calibration else "Generic lens geometry")
                        + " · Drag to aim. Dragging or zooming sets a key."
                    )
            elif msg[0] == "thumb-error":
                self.thumb_job = None
                self.status.set("Preview: " + msg[1])
                self.play = False
            elif msg[0] == "progress":
                total = self.clip.frames if msg[1] == "render" else self.clip.frames * 8 / self.clip.fps
                self.status.set(
                    ("Rendering" if msg[1] == "render" else "Preparing playback")
                    + f"  {min(100, int(msg[2] * 100 / total))}%"
                )
            else:
                self.background = None
                self.export_button.config(state="normal")
                self.stop.config(state="disabled")
                if msg[0] == "failed":
                    self.status.set("Job cancelled." if isinstance(msg[2], Cancelled) else "Job failed.")
                    if not isinstance(msg[2], Cancelled):
                        messagebox.showerror("Osmo 360 Editor", str(msg[2]))
                elif msg[1] == "proxy":
                    self.proxy = msg[2]
                    self.status.set("Preview ready.")
                    self.play_pause()
                else:
                    self.result = msg[2]
                    self.status.set("Exported " + str(self.result))
                    if self.output or getattr(self, "integrated", False):
                        self.close()
                        if self.closing:
                            return
        if self.play and self.thumb_job is None and not self.pending:
            n = self.start_frame + round((time.monotonic() - self.start_time) * float(self.clip.fps))
            if n >= self.clip.frames - 1:
                self.play = False
            self.move_to(n)
        self.play_button.config(text="Pause" if self.play else "Play")
        if self.thumb_job is None and self.pending:
            self.start_thumb()
        self.root.after(25, self.tick)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", nargs="?", type=Path)
    parser.add_argument("--output", type=Path, help="Destination provided by a Kdenlive Media Job")
    parser.add_argument("--integrated", action="store_true", help="Return the completed clip to KdenCut")
    args = parser.parse_args()
    root = tk.Tk()
    root.withdraw()
    source = args.source or filedialog.askopenfilename(
        filetypes=[("Osmo footage or edit", "*.OSV *.osv *.k360"), ("All files", "*")]
    )
    if not source:
        return 2
    with tempfile.TemporaryDirectory(prefix="osmo360-editor-") as folder:
        try:
            editor = Editor(root, Path(source), args.output, folder)
        except Exception as exc:
            messagebox.showerror("Open Osmo 360 clip", str(exc))
            return 1
        editor.integrated = args.integrated
        if args.integrated:
            editor.export_button.configure(text="Render and import")
        root.deiconify()
        root.mainloop()
        if editor.result:
            print(json.dumps({"clip": str(editor.result)}))
            return 0
        return 2


if __name__ == "__main__":
    sys.exit(main())
