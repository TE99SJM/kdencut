# KdenCut

KdenCut is a touch-friendly fork of [Kdenlive](https://kdenlive.org), with a
CapCut-style phone workspace and the existing Kdenlive editing engine and project
format. It runs natively or in a browser through the [Docker setup](kdencut-docker/README.md).

## Phone and tablet workspace

Touch Mode replaces the desktop docks with a preview, a touch timeline, and a
swipeable tool strip. Portrait stacks the preview above the timeline; landscape
places them side by side. The preview starts at the top of the editor, with no
desktop menu, workspace tabs or toolbar above it. Home, Media, Save and Export
sit below the timeline, above the swipeable icon-and-label editing strip.
Playback and undo/redo sit below the preview. Home offers a new video, opening projects and
continuing the current edit. Home > Desktop controls restores the desktop layout.
Saved desktop toolbar settings cannot bring the top bars back in Touch Mode;
switching to Desktop controls restores their previous visibility.

- Swipe the timeline to scrub under the centered playhead. Tap a clip to select
  it, hold and drag to move it, and drag its edge handles to trim. These operations
  use Kdenlive's undo history. Empty tracks stay out of the way.
- **Edit** contains Split, Delete, Duplicate, Speed, Volume, Fit and zoom controls.
  Speed opens the selected clip's existing speed dialog, including pitch options.
  Volume opens the selected audio clip's gain controls, reusing its Volume effect.
  For linked video/audio, the audio half is used; locked tracks must be unlocked.
- **Ratio** switches the project canvas between 9:16 (1080×1920), 1:1 (1080×1080)
  and 16:9 (1920×1080). The exact project frame rate is preserved. Check framing
  after changing the canvas and save the project; this is a project-wide setting.
- Audio, Text and Overlay imports use an empty matching track, adding a track when
  needed. Effects, Filters, Adjust and Transitions open scrollable panels.
- **Captions** opens speech recognition. The Docker image includes CPU Whisper and
  the multilingual `tiny` model, so no installation or model download is needed
  for the first job. Select the language and generate subtitles from the timeline.
  Review the transcription before exporting. Larger models are optional.
- **Export > TikTok / Reels / Shorts** starts an MP4 export at 1080×1920 with H.264,
  AAC stereo, progressive video and fast-start playback. It keeps the project
  frame rate and uses Kdenlive's vertical export framing. Check the preview in
  9:16 first for predictable framing. These are convenient video presets, not
  automatic uploads or guarantees about platform duration/file-size rules.
  In Docker, files go to `/media/exports` with unique names. The tested hardware
  encoder is used when available; otherwise exports use software. **Advanced**
  opens all normal export settings, including software presets and other sizes.

The browser translates touches to mouse input. Use the phone workspace's swipe,
trim and zoom buttons; native two-finger timeline gestures also work on a directly
attached touchscreen. Pinching an empty native timeline now keeps a finite zoom.
Project Settings uses a section dropdown and vertically stacked fields. Project
presets and the project-folder choice also use dropdowns; existing frame-rate,
scanning, audio-channel and codec selectors keep their normal behavior. OK and
Cancel stay below the scrolling page. Swipe up/down for more settings, including
proxy controls. The phone tool buttons and keyboard button have larger targets;
numeric fields have separate full-height increase/decrease buttons.
Other large legacy dialogs scroll to keep their buttons reachable on narrow screens.

## Media, uploads and NAS

**Media > Import**, Add video, Audio and Overlay open the vertical browser media
chooser. **This device > Photos / Videos** invokes the phone's standard media
library picker; **Browse Files** opens its file picker. Multiple files are supported.
Uploads use persisted 1 MiB chunks with progress, automatic retries and a manual
retry button. Keep the import/project open; after reloading the page, reselect the
same files to resume that pending import. Closing the project or cancelling the
import discards its unfinished copies.

**Server / NAS** browses `/media`, previous imports and saved SMB connections in a
single vertical list. **+ NAS** adds an SMB2/SMB3 share with optional credentials.
Selected NAS files are downloaded into persistent local import storage before
editing. Originals on the NAS are read only. Existing host-mounted NFS or other
storage can be used through `/media`; KdenCut does not mount NFS itself.

**Storage** shows free space and an unused-import review. Cleanup checks saved
projects, backups/autosaves and the current open project, then moves selected
unused imports to recoverable Trash. Permanent deletion requires a second usage
check and confirmation. See the [storage scope and recovery rules](kdencut-docker/README.md#storage-cleanup)
 before using it. Trash does not free disk space until permanently deleted.

## Appearance and translations

The KdenCut color scheme uses dark panels, cyan accents, teal video clips and green
audio clips. Touch controls have large targets and swipeable lists. Other KDE color
themes remain available under Settings > Color Theme. Input fields and selections
now use palette colors, including when switching between dark and light themes.
Phone labels use KDE's `i18n()`/gettext system; new strings require translated
catalog entries before they appear in a particular language.

The phone/tablet browser hides both the native cursor and Selkies’ separate mouse
pointer layer, including after reconnecting. Touch input remains active. Desktop
browsers keep their cursor behavior; use View > Show Mouse Cursor, or launch
`kdenlive --show-cursor` for the native editor. View > Touch Mode switches workspaces and remembers the
choice. Project saves preserve the desktop layout for later use.

## Docker capabilities

The image builds MLT with stabilization (`vidstab`), RNNoise audio denoising,
OpenCV motion tracking and Glaxnimate/Lottie playback. Lottie files are accepted by
the media chooser. The MLT playback module is included; the separate Glaxnimate
animation authoring application is not. These features remain available through
Kdenlive's normal clip/effect controls.

Optional [GPU overrides](kdencut-docker/README.md#optional-gpu-support) expose
Intel/AMD VAAPI or NVIDIA devices. Startup tests real encoder access and configures
preview decoding and social exports, falling back to software when the probe
fails. This does not make every effect or timeline composition run on the GPU.

## Building and checking

Use [Kdenlive's build guide](dev-docs/build.md), or the Dockerfile, which installs
the required Qt/KDE versions, builds MLT and runs QML lint during compilation.
Native builds do not automatically install Whisper; the ready-to-use speech
runtime belongs to the Docker image.

The branch `kdencut-phone-completion` starts from `kdencut-touch-capcut` and merges
KDE/kdenlive master at `00141d89668d2519d48cc12b0b001ae34b72cf42` (115 upstream commits).
GitHub Actions builds the full image, compiles the native tests, exercises browser
uploads and a real SMB share, and checks packaged MLT services and offline Whisper.

## Where the changes live

| Location | Purpose |
| --- | --- |
| `src/kdencut/mobileworkspace.cpp` | Responsive phone layout, tools and translated labels |
| `src/kdencut/mobilesettings.cpp`, `touchscrolling.h` | Vertical settings, dropdown adapters and kinetic scrolling |
| `src/kdencut/mainwindowtouch.cpp` | Dock ownership, lifecycle connections and phone commands |
| `src/kdencut/mobileediting.cpp`, `mobileexport.cpp`, `phonepresets.h` | Volume, canvas ratios and social export integration |
| `src/kdencut/touchmode.cpp`, `palettewatcher.h`, `kdencut-touch.qss` | Touch behavior, cursor and palette-aware styling |
| `src/kdencut/mediaimportbridge.cpp`, `mediaprojectusage.cpp` | Browser import handoff and open-project usage snapshot |
| `src/kdencut/runtimefeatures.cpp` | Validated GPU and first-run speech settings |
| `src/timeline2/view/qml/TouchTimeline.qml`, `Timeline.qml` | Touch timeline and native zoom gestures |
| `kdencut-docker/root/usr/local/share/kdencut/` | Browser chooser, transfer service, cleanup and runtime probes |
| `kdencut-docker/tests/` | Transfer, storage, GPU, captions, translation and UI regressions |

Most phone integration is isolated under `src/kdencut/`; `mainwindow.cpp` retains
small lifecycle/dock hooks to make later upstream merges easier.

