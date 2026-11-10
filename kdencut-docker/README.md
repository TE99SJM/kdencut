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
The logical desktop stays at least 960×640 at 96 DPI so native dialogs are not
larger than the screen. Selkies scales the stream and pointer coordinates together.
Administrator-pinned resolution/DPI settings are respected.

In touch mode, narrow or portrait desktops put the bin, monitors, effects and
composition stack in one tabbed workspace above the timeline. The menu is available
through the toolbar's hamburger button. A wider landscape screen restores the
previous dock arrangement; saving a project retains that desktop arrangement.
Quick Setup reflows and scrolls, while its Start Editing button stays below the form.

Rebuild the image after pulling this change and reload the browser page to load the
new viewport adapter. No configuration reset is needed. This does not install the
optional Glaxnimate/Lottie module shown in the startup warning.

Developer checks:

```bash
node --test kdencut-docker/tests/test_mobile_logic.cjs
python kdencut-docker/tests/check_splash.py        # requires PySide6
python kdencut-docker/tests/test_mobile_viewport.py # requires Playwright + Chromium
```

The browser checks cover container geometry and the Selkies message contract with
a simulated stream; a rebuilt container and an iPhone are still needed for end-to-end
validation of native docks, video streaming and touch input.
