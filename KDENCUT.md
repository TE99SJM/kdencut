# KdenCut

KdenCut is a touch-friendly fork of [Kdenlive](https://kdenlive.org) with a CapCut-style look.
All Kdenlive features and project files (`.kdenlive`) keep working.

## What is different from Kdenlive

### CapCut-style look
- New **KdenCut** color scheme: near-black panels, cyan accent (`#00C1CD`), white selection outline on clips,
  teal video clips and green audio clips in the timeline.
- Rounded buttons, tabs with a cyan underline, wide rounded scroll bars, tall menu rows.
- Round play button under the preview monitor.
- CapCut-style timeline toolbar: **Undo, Redo, Split, Delete, Duplicate** come first.
- The color scheme is applied automatically on first start (and once for people upgrading from Kdenlive).
  You can still pick another one in **Settings > Color Theme**.

### Touch friendly
- Buttons, tabs, list rows, sliders, check boxes and menus are sized for fingers (40 px targets).
- Toolbar icons are at least 28 px.
- Timeline handles (trim, fades, track heads) are 35% bigger and tracks default to 64 px tall.
- **Swipe to scroll** in lists, settings pages, the effect stack and other scroll areas.
  Lists you drag items out of (Project Bin, Effects, Compositions) keep drag-and-drop, so you can still
  drag clips and effects onto the timeline with your finger.
- **Timeline gestures:** pinch with two fingers to zoom, drag with two fingers to scroll left/right and up/down.
  Touchpad pinch works too.

### No mouse cursor
- The mouse pointer is hidden by default, including over the timeline and monitors.
- Bring it back with **View > Show Mouse Cursor**, or start with `kdenlive --show-cursor` for one session.

### Turning touch mode off
**View > Touch Mode** switches the large controls, swipe scrolling and pinch gestures on or off.
Stylesheet and scrolling change right away; timeline handle sizes update after restarting KdenCut.

## Building

Same as Kdenlive, see [dev-docs/build.md](dev-docs/build.md). Short version on Ubuntu 26.04 / Debian testing:

```bash
sudo apt build-dep kdenlive
sudo apt install libkddockwidgets-qt6-dev libopentimelineio-dev libimath-dev ninja-build
# Kdenlive master needs MLT >= 7.42; build it from https://github.com/mltframework/mlt if your distro is older
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=$HOME/kdencut
ninja -C build
ninja -C build install
$HOME/kdencut/bin/kdenlive
```

## Where the changes live

| File | Purpose |
| --- | --- |
| `src/kdencut/touchmode.cpp/.h` | Applies the theme, touch stylesheet, swipe scrolling, toolbar sizes and cursor hiding |
| `src/kdencut/KdenCut.colors` | CapCut-style KDE color scheme (also installed to `share/color-schemes`) |
| `src/kdencut/kdencut-touch.qss` | Touch stylesheet (sizes, rounded corners, accent color) |
| `src/timeline2/view/qml/Timeline.qml` | Pinch-to-zoom and two-finger scrolling |
| `src/utils/uiutils.cpp` | Bigger QML size unit in touch mode |
| `src/kdenliveui.rc`, `src/mainwindow_actions.cpp` | Touch Mode and Show Mouse Cursor menu items, CapCut-style timeline toolbar |
| `src/kdenlivesettings.kcfg` | `touchmode`, `showcursor` and `kdencutthemeversion` settings |
