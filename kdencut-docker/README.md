# KdenCut in Docker

KdenCut runs in a Selkies web desktop on Ubuntu 26.04. Open it from a phone, tablet
or desktop browser; audio plays in the browser. See the [workspace overview](../KDENCUT.md).

## New installation

```bash
git clone -b kdencut-phone-completion https://github.com/TE99SJM/kdencut.git ~/kdencut-src
cd ~/kdencut-src/kdencut-docker
cp .env.example .env
nano .env
docker compose build
docker compose up -d
```

For a new installation, set PUID/PGID to the owner of the storage folders (`id`),
choose CONFIG_DIR and MEDIA_DIR, and configure CUSTOM_USER/PASSWORD. Existing
installations keep their `.env`. Defaults remain HTTP **3006 → 3000**, HTTPS
**3007 → 3001**, and the same `/config` and `/media` volume mappings.
Open **https://SERVER-IP:3007** and accept the local certificate if appropriate.
Keep the web desktop behind your private network/Tailscale or suitable access
controls: it includes a server terminal. The media API uses the editor's login.

The first build downloads build dependencies, CPU PyTorch, Whisper and its tiny
model, then compiles MLT and KdenCut. Allow 30–90 minutes, several GB of downloads
and substantial Docker build space. If memory is limited, use
`docker compose build --build-arg BUILD_JOBS=2`; no `.env` edit is needed.

## Update an existing server

Save and close the project before recreating the container. From the checkout
shown in the server screenshots:

```bash
cd ~/kdencut-src
git fetch origin
git switch kdencut-phone-completion
git pull --ff-only origin kdencut-phone-completion
cd kdencut-docker
docker compose build --build-arg BUILD_JOBS=2
docker compose up -d --force-recreate
docker compose ps
```

The first `git switch` creates a local tracking branch from the fetched remote.
If Git reports local tracked changes, preserve them and resolve that warning;
do not reset or clean the checkout. This update does not replace `.env` or clear
settings, projects or media. Keep any existing local Compose overrides/mounts.
Reload the browser page after rebuilding; browser assets have versioned URLs.
A build alone does not replace the running container.

If using the generated GPU override below, use both `-f` files for every Compose
build/up/down command. To roll back the code, switch to `kdencut-touch-capcut`,
rebuild and recreate using the same volumes. Back up project files before testing
new upstream versions; returning to older code does not undo saved project edits.

## Phone workspace

Portrait places preview above timeline; landscape puts them side by side. Media,
Save and Export remain at the top. Swipe the tool strip for additional tools.
Edit offers Split, Delete, Duplicate, Speed, Volume, Fit and zoom. Tap a clip first;
volume uses its audio selection. Ratio offers 9:16, 1:1 and 16:9 while preserving
frame rate. Audio, titles and overlays insert on an empty matching track.

Swipe to scrub, hold a clip then drag to move, and drag its large edge handles to
trim. Browser input is mouse-compatible; use zoom buttons in the streamed editor.
Native touchscreen pinch stays safe even with an empty timeline. Secondary panels
and large desktop dialogs scroll within the phone viewport. The keyboard button
has its own bottom strip; rotation and the software keyboard resize the stream.
Administrator-pinned display settings are respected.

Home > Desktop controls restores the desktop docks. View > Touch Mode returns to
the phone layout. View > Show Mouse Cursor reveals the normally hidden pointer.
Settings > Color Theme supports both dark and light palettes, including inputs.

**Export > TikTok / Reels / Shorts** starts a 1080×1920 progressive H.264/AAC MP4
in `/media/exports`, with a unique filename, stereo 48 kHz audio and fast-start
playback. The project frame rate is retained. Kdenlive's vertical export framing
is used without changing the project canvas; use Ratio > 9:16 to review composition
before exporting. These presets do not publish to social accounts. Normal warnings
about variable-frame-rate clips, muted audio or missing media may still require
attention. Advanced opens the normal export dialog and software/GPU preset choices.

## Media chooser and phone uploads

Media > Import, Add video, Audio and Overlay open a portrait-friendly browser
chooser. Source and folder controls sit above a vertical list; filenames wrap,
sizes remain readable and Add selected stays visible while scrolling.

- **This device > Photos / Videos** opens the phone's standard library picker.
- **Browse Files** opens its file picker, including providers available on the phone.
- **Server / NAS** browses `/media`, previous imports and saved SMB connections.
- **Open server file picker** provides the original server-side dialog.

Uploads use **1 MiB chunks**, a progress bar and offsets persisted on the server.
A dropped connection triggers three retries with increasing delays. If it stays
offline, restore connectivity and tap **Retry upload**. The server acknowledges
already-written chunks without duplicating them. After a page reload, reselect
the same files to resume the still-open import; the browser cannot retain access
to phone files across reloads. File identity uses metadata and sampled bytes for
resume matching, not a full-file integrity checksum.

Keep the project/import open until completion. Cancelling removes unfinished
copies; closing or switching the project invalidates its pending handoff. Server
transfer metadata survives a media-service restart, but a full editor/container
restart closes the native import request, so reselect/start a new import then.
Each file is limited to 100 GiB and checked against available space. Files stream
to disk; only one bounded chunk is buffered at a time.

## NAS imports

Use **+ NAS** for an SMB2/SMB3 hostname or IPv4 address, share name, optional
username/password and optional nonstandard port (default 445). For `\\MNAS\Video`,
enter `MNAS` and `Video`. Credentials are validated and stored in an owner-only
server file; passwords are not returned to the browser. **Forget** removes the
connection without deleting imported media.

Selected NAS files download into local persistent import storage before editing.
Transfers show progress and can be cancelled. SMB downloads currently restart
after failure; chunked resume applies to phone uploads. NAS originals are never
changed or deleted. No privileged container or host SMB mount is required.

Files already under `/media` import in place. NFS and other host-mounted storage
work through your existing `/media` mapping or server file picker; the app does
not mount NFS itself. Use the host's existing mount/permissions and retain your
Compose overrides. Hostname resolution must work from inside the container.

## Storage cleanup

Uploads and downloaded NAS files live in
`CONFIG_DIR/KdenCut/Imports` (`/config/KdenCut/Imports` inside Docker), each in a
unique folder. Moving or deleting these files manually can break projects.
The chooser displays available import-storage space, with **Low storage** below
2 GiB or 5% of that filesystem's total capacity, whichever is larger. This is the
`/config` filesystem; exports under `/media` may use another filesystem.

Open **Server / NAS > Storage** to review unused imports:

1. Confirm that every project using these imports is stored under `/config` or
   `/media`. Projects on disconnected storage, elsewhere on the server, in archives,
   or on other machines cannot be checked. Bring them into those locations first,
   or keep the imports. The same applies to historical copies you intend to restore.
2. Review unused imports. The service checks `.kdenlive`/`.mlt` files, project
   backups/autosaves and a fresh snapshot of the open project's bin, including
   unsaved clips. Unreadable/malformed projects, unavailable locations, linked
   project folders or an unavailable editor block cleanup.
3. Select files and move them to **recoverable Trash**. Usage is checked again.
   Only application-owned imports at least 24 hours old are eligible. Active
   transfers, symlinks, unexpected folder contents and legacy unmarked imports
   are excluded. Server files imported in place and NAS originals are excluded.
4. Use **Recover deleted imports** to restore their original paths. Trash consumes
   space until **Permanently delete trash** is confirmed. That operation rechecks
   project usage again and cannot be undone. There is no automatic purge.

Cleanup cannot inspect arbitrary external projects or guarantee future undo/history
operations will not want an older asset. Keep recoverable copies until satisfied;
restore a trashed import before reopening an older project that needs it.

## Optional GPU support

The base Compose file works without a GPU. Its ports, volumes and `.env` settings
are unchanged. Generate an optional override **on the Docker host**:

```bash
cd ~/kdencut-src/kdencut-docker
python3 configure-gpu.py
docker compose -f compose.yaml -f compose.gpu.generated.yaml config --quiet
docker compose -f compose.yaml -f compose.gpu.generated.yaml up -d --force-recreate
```

Run the detector as a user who can run `docker info`. It chooses NVIDIA only when
`nvidia-smi` works and Docker has the NVIDIA runtime, otherwise Intel/AMD render
nodes under `/dev/dri`, otherwise software. The generated file is ignored by Git.
It adds GPU devices/runtime/environment only. Use the same two `-f` arguments for
future builds and updates. Regenerate the override after hardware/driver changes;
Docker cannot start a stale device/runtime mapping before the app can fall back.

| Backend | Host prerequisites | Compose additions |
| --- | --- | --- |
| Intel / AMD VAAPI | Working host DRM driver and `/dev/dri/renderD*` | `/dev/dri` device mapping, render-node selection |
| NVIDIA | NVIDIA driver and NVIDIA Container Toolkit configured for Docker | NVIDIA runtime, GPU reservation and driver capabilities |
| Software | None | No device mapping or runtime requirement |

For manual configuration, [compose.vaapi.yaml](compose.vaapi.yaml) and
[compose.nvidia.yaml](compose.nvidia.yaml) are examples. Use one alongside
`compose.yaml`. The VAAPI example selects `renderD128`; adjust it if the desired
node is different. The detector selects the first available render node, while
the editor probe tries the mapped nodes. For NVIDIA browser-stream acceleration,
a corresponding DRM render node may also be needed; the generated override maps
`/dev/dri` when present. Host drivers/toolkit installation is separate from building
this image. See [Docker GPU support](https://docs.docker.com/compose/how-tos/gpu-support/)
and [Selkies GPU requirements](https://docs.linuxserver.io/selkies/user-guide/gpu/).

At editor startup a real FFmpeg H.264 encode runs as the editor user. Only a working
encoder enables the corresponding MLT preview decoder and social export encoder.
A failed probe clears stale acceleration settings and uses libx264/software.
Unsupported source codecs can fall back to software decoding. Effects, compositing
and CPU Whisper are not all GPU-accelerated. Check the selected backend with:

```bash
docker exec kdencut cat /config/.local/share/kdencut/hardware.json
```

To force software without editing `.env`:

```bash
python3 configure-gpu.py --mode software
docker compose -f compose.yaml -f compose.gpu.generated.yaml up -d --force-recreate
```

An encoder can still fail later because of a driver reset or format limitation;
select a software preset in Advanced export in that case. Startup probing is not
an automatic retry of failed render jobs.

## Packaged effects and captions

| Capability | Included implementation | Where to use it |
| --- | --- | --- |
| Stabilization | MLT vidstab | Kdenlive clip stabilization job |
| Audio noise reduction | RNNoise 0.2 + MLT rnnoise | RNNoise audio effect |
| Motion tracking | MLT OpenCV tracker | Motion tracker effect |
| Lottie playback | MLT Glaxnimate Qt6 module | Import Lottie/JSON animation files |
| Auto-captions | OpenAI Whisper + multilingual tiny model | Phone Captions button |

MLT's pinned Glaxnimate source and RNNoise are built in the image. The separate
Glaxnimate authoring GUI is not installed. Accepted animation extensions include
`.json`, `.lottie`, `.rawr`, `.tgs` and `.lot`; actual format support depends on the
packaged Glaxnimate importer. Experimental MOVIT remains disabled.

Whisper has an isolated Python environment in `/opt/kdencut-speech`; its bundled
model lives in `/opt/kdencut-models` and is linked into the persistent Whisper cache.
The first upgrade selects CPU, tiny and FP32, with full-timeline captioning.
Later user model/language/device preferences are preserved. Larger models can be
chosen in Kdenlive's speech settings and require download/storage/time. CPU tiny
is intended to work on GPU-free hosts, not to give the best transcription quality.

## Automated checks and manual acceptance

GitHub Actions builds the production image, including application QML lint and
C++ compilation. It runs unit tests for uploads, cleanup and GPU fallback; browser
tests for native file-picker activation, dropped upload replies and vertical
layout; a real SMB share test; native Qt tests for workspace behavior, themes,
gettext translations and presets; and an offline Whisper/MLT packaging test.

Useful local checks from the repository root:

```bash
node --test kdencut-docker/tests/test_*.cjs
python3 -m unittest discover -s kdencut-docker/tests -p test_resumable_uploads.py -v
python3 -m unittest discover -s kdencut-docker/tests -p test_import_cleanup.py -v
python3 -m unittest discover -s kdencut-docker/tests -p test_gpu.py -v
python3 -m unittest discover -s kdencut-docker/tests -p test_docs.py -v
cmake -S kdencut-docker/tests/native -B /tmp/kdencut-mobile-checks
cmake --build /tmp/kdencut-mobile-checks
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ctest --test-dir /tmp/kdencut-mobile-checks --output-on-failure
```

Native checks require Qt6 Widgets/QuickWidgets/Test/Xml, KF6 I18n and Python.
Browser checks additionally require Playwright/Chromium; SMB checks require
smbprotocol and impacket. The workflow records exact setup commands.

On a real phone **and** tablet, check portrait/landscape and keyboard resizing;
Photos/Files upload with a brief network interruption; a real NAS download;
trim/split/speed/volume with undo; all three ratios; caption generation from speech;
and each social export (including sound and downloaded playback). Test a light
color theme. Review/restore an eligible unused import and verify a referenced one
is protected. Check GPU status and a short export on each host you intend to use;
CI does not have your GPU, iOS photo library or NAS.
