/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
(() => {
  // Mobile remote desktops need enough logical pixels for a video editor.
  // Use Selkies' own fit and input mapping; never scale just its video canvas.
  const touch = window.matchMedia('(pointer: coarse)');
  const root = document.documentElement;
  let settings = null;
  let timer;
  let previousSize = '';
  const viewer = /^#(shared|player[234])(?:[?&]|$)/.test(location.hash);
  const send = (message) => window.postMessage(message, location.origin);

  function update() {
    const active = touch.matches && !viewer;
    root.classList.toggle('kdencut-mobile', active);
    if (!active) return;
    const viewport = window.visualViewport;
    const width = viewport ? viewport.width : window.innerWidth;
    const height = viewport ? viewport.height : window.innerHeight;
    const top = viewport ? viewport.offsetTop : 0;
    const left = viewport ? viewport.offsetLeft : 0;
    for (const [key, value] of Object.entries({ width, height, top, left,
      bottom: Math.max(0, window.innerHeight - height - top) })) {
      root.style.setProperty(`--kdencut-${key}`, `${value}px`);
    }
    const container = document.querySelector('.video-container');
    if (!settings || !container) return;
    const box = container.getBoundingClientRect();
    if (box.width <= 0 || box.height <= 0) return;
    // A visible keyboard should reduce the display area, not crop its bottom.
    const scale = Math.max(1, 960 / box.width, 640 / box.height);
    const w = Math.ceil(box.width * scale / 2) * 2;
    const h = Math.ceil(box.height * scale / 2) * 2;
    const size = `${w}x${h}`;
    if (size === previousSize) return;
    previousSize = size;
    send({ type: 'setScaleLocally', value: true });
    // Respect an administrator's fixed resolution or DPI policy.
    const lockedSize = settings.enable_resize?.value === false ||
      settings.manual_width?.value > 0 || settings.manual_height?.value > 0;
    if (!lockedSize) send({ type: 'setManualResolution', width: w, height: h });
    // Send after manual resolution: its derived HiDPI default must not enlarge
    // the Qt UI again, undoing the minimum logical workspace above.
    const dpi = settings.scaling_dpi;
    if (!dpi?.locked && (!dpi?.allowed || dpi.allowed.some((value) => Number(value) === 96))) {
      send({ type: 'settings', settings: { scaling_dpi: 96 } });
    }
  }
  function schedule() {
    clearTimeout(timer);
    timer = setTimeout(update, 120);
  }
  window.addEventListener('message', (event) => {
    if (event.source !== window || event.origin !== location.origin ||
        event.data?.type !== 'serverSettings') return;
    settings = event.data.payload || {};
    previousSize = ''; // A reconnect needs the dimensions sent again.
    schedule();
  });
  window.addEventListener('resize', schedule);
  window.addEventListener('orientationchange', schedule);
  window.visualViewport?.addEventListener('resize', schedule);
  window.visualViewport?.addEventListener('scroll', schedule);
  touch.addEventListener('change', schedule);
  new ResizeObserver(schedule).observe(document.getElementById('app') || document.body);
  update();
})();
