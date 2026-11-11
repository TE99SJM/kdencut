# KdenCut on a server (Docker, use it in a browser)

Runs KdenCut inside a web desktop (linuxserver Selkies, Ubuntu 26.04) so you can edit from
any browser or touchscreen tablet on your network. Sound plays in the browser.

## Install

```bash
git clone -b kdencut-touch-capcut https://github.com/TE99SJM/kdencut.git ~/kdencut
cd ~/kdencut/kdencut-docker
cp .env.example .env
nano .env                    # set PUID/PGID (run: id), PASSWORD, MEDIA_DIR
docker compose up -d --build # first build compiles MLT + KdenCut: 30-90 minutes
docker compose logs -f       # Ctrl+C to stop watching
```

Open **https://SERVER-IP:3007** (accept the self-signed certificate warning).
Your videos are in `/media` inside the file dialogs.

## Update after new commits

```bash
cd ~/kdencut && git pull
cd kdencut-docker && docker compose up -d --build
```

Settings and projects in `CONFIG_DIR` are kept.

## Remove

```bash
cd ~/kdencut/kdencut-docker && docker compose down
```

## Notes
- If the build is killed (out of memory), set `BUILD_JOBS=2` in `.env` and run the build again.
- The mouse cursor is hidden on purpose. Right-click the desktop (outside KdenCut) and choose
  **KdenCut (show cursor)**, or use **View > Show Mouse Cursor**.
- Through the browser, taps act as clicks. Two-finger pinch-zoom on the timeline needs a
  touchscreen attached directly to a Linux PC; in the browser use Edit > Zoom in / Zoom out.
- The web desktop includes a terminal with root access. Keep it on your home network or
  Tailscale, and always set `CUSTOM_USER` and `PASSWORD`.

## Phone display fixes

### Phone uploads and NAS storage

In the touch workspace, **Media > Import**, **Add video**, **Audio**, and **Overlay**
open a portrait-friendly browser media chooser on **Server / NAS**. Storage and
folder controls sit above a single vertical file list, with file sizes under
wrapping filenames and a full-width **Add selected** button that stays in view
while scrolling. **This device > Photos / Videos** uses the phone's
native library picker; **Browse Files** opens the device's standard file picker.
Select several files at once. Progress is shown while their bytes upload to the
server, and completed files are added to the project/timeline. Cancelling the
native picker leaves the chooser open. Cancelling an import removes unfinished
copies. Keep the project open until the import finishes.

**Server / NAS** lists the existing `/media` mount and prior uploaded/downloaded
media. Use **+ NAS** to save an SMB2/SMB3 connection: a hostname or IPv4 address,
share name, and optional username/password. For `\\\\MNAS\\Video`, enter `MNAS`
as the address and `Video` as the share. Credentials are checked before saving,
kept in an owner-only server file, and never returned to the browser. **Forget**
removes the saved connection without deleting imported media.

Direct SMB connections default to port 445 (custom ports are supported) and download selected files into persistent
local storage before importing them. No host mount or privileged container is
needed. Files already mounted under `/media` can be imported in place. NFS and
other host-mounted storage remain usable through that mount or **Open server file
picker**. The app does not mount NFS shares itself.

Uploads and SMB copies live under `CONFIG_DIR/KdenCut/Imports`, each in a unique
folder so equal filenames never overwrite one another. Retain those files while
a project references them. NAS originals are only read, never changed or deleted.
Insufficient disk space, disconnected shares, and failed transfers are reported
in the chooser. Each file can be up to 100 GiB; available server disk space is
checked before transfer. Uploads are streamed to disk rather than held in RAM.

The chooser and its API use the same origin and login as the editor. Rebuild and
recreate the container, then reload the browser after updating. The browser tests
exercise native file-input activation, multiple file uploads, folder selection,
phone sizing, and cancellation. CI also tests a real local SMB2 share and the
compiled native project-lifetime bridge; an iPhone and the actual NAS still need
an installation check.

Touch clients reserve a separate bottom strip for the keyboard button and fit the
stream to Safari's visible viewport, including rotation and the software keyboard.
The streamed desktop now uses phone-sized logical pixels at 96 DPI (minimum
320×280). It no longer creates a 960-pixel desktop and shrinks its controls.
Selkies scales the stream and pointer coordinates together. Administrator-pinned
resolution/DPI settings are respected.

Touch Mode uses the full-screen editing workspace at every screen size. The project preview sits above the timeline in
portrait; landscape places them side by side. The swipeable bottom strip contains
Edit, Audio, Text, Effects, Overlay, Captions, Filters, Adjust and Transitions.
Media, Save and Export stay at the top, with playback and undo/redo below the preview.
Edit opens Split, Delete, Duplicate, Fit and zoom controls. Importing media inserts it into the active
matching track; the Media sheet can also insert selected bin clips.
Audio, Text and Overlay imports use an empty matching track, creating one when needed,
so added music and titles do not collide with existing clips.

The touch timeline keeps a white playhead centered while thumbnail strips scroll
under it. Swipe to scrub with momentum; tap a clip to select it. Hold a clip before
dragging to move it, or drag its large white edge handles to trim. Each completed
move or trim uses the existing undoable timeline operation. Empty tracks are hidden;
Add video, Add audio and Add text remain directly accessible. Track scrolling and
tool strips accept mouse drags from the browser as well as direct touch.

The mobile home has New video, portrait/landscape format selection, Open project,
and recent projects. Home inside the editor offers Continue editing and Desktop
controls. Secondary dock panels open as scrollable sheets. Oversized desktop
forms, including export settings, scroll within the available screen. Advanced
forms still use their existing Kdenlive controls.

Home > Desktop controls restores the previous desktop layout. Upgrading to this
reskin enables Touch Mode once for existing profiles; a later Desktop choice is
remembered. Project saves retain that desktop layout; loading projects and changing
project aspect ratio cannot restore desktop docks over the phone workspace.
Quick Setup and recovery screens remain available.

Rebuild the image after pulling this change and reload the browser page to load the
new viewport adapter. Asset URLs are versioned so the browser fetches changed files. No configuration reset is needed. This does not install the
optional Glaxnimate/Lottie module shown in the startup warning.

Developer checks:

```bash
node --test kdencut-docker/tests/test_mobile_logic.cjs
python kdencut-docker/tests/check_splash.py        # requires PySide6
python kdencut-docker/tests/test_mobile_viewport.py # requires Playwright + Chromium
cmake -S kdencut-docker/tests/native -B /tmp/kdencut-mobile-checks # Qt6 Widgets + QuickWidgets + Test
cmake --build /tmp/kdencut-mobile-checks
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ctest --test-dir /tmp/kdencut-mobile-checks --output-on-failure
```

The browser checks cover container geometry and the Selkies message contract with
a simulated stream; a rebuilt container and an iPhone are still needed for end-to-end
validation of native docks, video streaming and touch input.

The native checks compile the actual mobile workspace and exercise portrait,
landscape, keyboard-height viewports, action enablement, panel ownership and
scrollable dialog acceptance. They also load the real touch QML with hierarchical
clip data and check swipe-to-scrub, hold-to-move and trim-on-release. GitHub Actions also builds the production Dockerfile,
which runs the application's QML lint and C++ compilation before running these checks.
