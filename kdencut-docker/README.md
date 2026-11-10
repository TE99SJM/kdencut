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
  touchscreen attached directly to a Linux PC; in the browser use the timeline zoom slider.
- The web desktop includes a terminal with root access. Keep it on your home network or
  Tailscale, and always set `CUSTOM_USER` and `PASSWORD`.

## Phone display fixes

Touch clients reserve a separate bottom strip for the keyboard button and fit the
stream to Safari's visible viewport, including rotation and the software keyboard.
The streamed desktop now uses phone-sized logical pixels at 96 DPI (minimum
320×280). It no longer creates a 960-pixel desktop and shrinks its controls.
Selkies scales the stream and pointer coordinates together. Administrator-pinned
resolution/DPI settings are respected.

With Touch Mode enabled, screens narrower than 1100 pixels or in portrait use a
full-screen mobile workspace. The project preview sits above the timeline in
portrait; landscape places them side by side. A swipeable bottom strip opens Media,
Audio, Text, Effects, Adjust, Captions and Transitions. Save and Export stay at the
top, with playback and undo/redo below the preview. Timeline commands include Add,
Split, Delete, Duplicate and Fit. Importing media inserts it into the active
matching track; the Media sheet can also insert selected bin clips.

The mobile home has New video, portrait/landscape format selection, Open project,
and recent projects. Home inside the editor offers Continue editing and Desktop
controls. Secondary dock panels open as scrollable sheets. Oversized desktop
forms, including export settings, scroll within the available screen. Advanced
forms still use their existing Kdenlive controls.

A wide landscape screen or disabling Touch Mode restores the previous desktop
layout. Project saves retain that desktop layout; loading projects and changing
project aspect ratio cannot restore desktop docks over the phone workspace.
Quick Setup and recovery screens remain available.

Rebuild the image after pulling this change and reload the browser page to load the
new viewport adapter. No configuration reset is needed. This does not install the
optional Glaxnimate/Lottie module shown in the startup warning.

Developer checks:

```bash
node --test kdencut-docker/tests/test_mobile_logic.cjs
python kdencut-docker/tests/check_splash.py        # requires PySide6
python kdencut-docker/tests/test_mobile_viewport.py # requires Playwright + Chromium
cmake -S kdencut-docker/tests/native -B /tmp/kdencut-mobile-checks # Qt6 Widgets + Test
cmake --build /tmp/kdencut-mobile-checks
QT_QPA_PLATFORM=offscreen ctest --test-dir /tmp/kdencut-mobile-checks --output-on-failure
```

The browser checks cover container geometry and the Selkies message contract with
a simulated stream; a rebuilt container and an iPhone are still needed for end-to-end
validation of native docks, video streaming and touch input.

The native checks compile the actual mobile workspace and exercise portrait,
landscape, keyboard-height viewports, action enablement, panel ownership and
scrollable dialog acceptance. GitHub Actions also builds the production Dockerfile,
which runs the application's QML lint and C++ compilation before running these checks.
