/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
(() => {
  // Match phone CSS pixels to the native mobile workspace, at 96 DPI.
  // Use Selkies' own fit and input mapping; never scale just its video canvas.
  const touch = window.matchMedia('(pointer: coarse)');
  const root = document.documentElement;
  let settings = null;
  let timer;
  let previousSize = '';
  const viewer = /^#(shared|player[234])(?:[?&]|$)/.test(location.hash);
  const send = (message) => window.postMessage(message, location.origin);
  let cursorInput = null;
  let cursorObserver = null;
  let originalCursor = ['', ''];

  function restoreCursor() {
    cursorObserver?.disconnect();
    cursorObserver = null;
    if (cursorInput) {
      cursorInput.cursorDiv?.classList.remove('kdencut-remote-cursor');
      const style = cursorInput.element?.style;
      if (style) {
        if (originalCursor[0]) style.setProperty('cursor', ...originalCursor);
        else style.removeProperty('cursor');
      }
    }
    cursorInput = null;
  }

  function syncCursor() {
    const input = window.webrtcInput;
    if (!touch.matches || viewer || input !== cursorInput) restoreCursor();
    if (!touch.matches || viewer || !input?.element || !input?.cursorDiv) return;
    if (cursorInput) return;
    cursorInput = input;
    const style = input.element.style;
    originalCursor = [style.getPropertyValue('cursor'), style.getPropertyPriority('cursor')];
    // Selkies draws its pointer separately from the video, and may re-show it
    // on every tap. Hide that owned layer without touching its input handlers.
    input.cursorDiv.classList.add('kdencut-remote-cursor');
    const hideBrowserCursor = () => {
      if (style.getPropertyValue('cursor') === 'none' && style.getPropertyPriority('cursor') === 'important') return;
      originalCursor = [style.getPropertyValue('cursor'), style.getPropertyPriority('cursor')];
      style.setProperty('cursor', 'none', 'important');
    };
    cursorObserver = new MutationObserver(hideBrowserCursor);
    cursorObserver.observe(input.element, {attributes: true, attributeFilter: ['style']});
    hideBrowserCursor();
  }

  function update() {
    const active = touch.matches && !viewer;
    root.classList.toggle('kdencut-mobile', active);
    syncCursor();
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
    const scale = Math.max(1, 320 / box.width, 280 / box.height);
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
    // the Qt UI again, undoing the phone-sized workspace above.
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
  // The streaming input is recreated on reconnect, after this script loads.
  new MutationObserver(syncCursor).observe(document.body, {childList: true, subtree: true});
  new ResizeObserver(schedule).observe(document.getElementById('app') || document.body);
  update();
})();
