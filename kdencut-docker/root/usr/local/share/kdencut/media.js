/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
(() => {
  if (/^#(shared|player[234])(?:[?&]|$)/.test(location.hash)) return;
  const base = new URL('kdencut-media/', document.baseURI);
  let request = null, busy = false, xhr = null, source = '', path = '', browseSerial = 0;
  const selected = new Set();
  const el = (tag, text, parent, attrs = {}) => {
    const node = document.createElement(tag);
    if (text) node.textContent = text;
    for (const [key, value] of Object.entries(attrs)) node.setAttribute(key, value);
    if (parent) parent.append(node);
    return node;
  };
  const panel = el('dialog', '', document.body, {id: 'kdencut-media-dialog', class: 'allow-native-input', 'aria-labelledby': 'kdencut-media-heading'});
  const header = el('header', '', panel);
  const heading = el('h2', 'Add media', header, {id: 'kdencut-media-heading'});
  const close = el('button', 'Cancel', header, {type: 'button'});
  const tabs = el('nav', '', panel, {'aria-label': 'Media source'});
  const deviceTab = el('button', 'This device', tabs, {type: 'button'});
  const storageTab = el('button', 'Server / NAS', tabs, {type: 'button'});
  const content = el('section', '', panel);
  const device = el('section', '', content);
  el('p', 'Choose videos, photos, or audio from your device. Files are copied to server storage before they are added to your timeline.', device);
  const photos = el('label', 'Photos / Videos', device, {class: 'kdencut-picker'});
  // Keep a real file input in the tapped label: iOS needs a user gesture.
  const photoInput = el('input', '', photos, {type: 'file', accept: 'image/*,video/*', multiple: '', 'aria-label': 'Choose photos and videos'});
  const files = el('label', 'Browse Files', device, {class: 'kdencut-picker secondary'});
  const fileInput = el('input', '', files, {type: 'file', accept: 'video/*,image/*,audio/*,.mkv,.mts,.m2ts,.mxf,.heic,.heif,.mov', multiple: '', 'aria-label': 'Browse device files'});
  const storage = el('section', '', content, {hidden: '', class: 'kdencut-storage'});
  const sourceRow = el('div', '', storage, {class: 'kdencut-source-row'});
  el('label', 'Storage location', sourceRow, {for: 'kdencut-storage-location', class: 'kdencut-muted'});
  const sources = el('select', '', sourceRow, {id: 'kdencut-storage-location'});
  const sourceActions = el('div', '', sourceRow, {class: 'kdencut-source-actions'});
  const addNas = el('button', '+ NAS', sourceActions, {type: 'button'});
  const forget = el('button', 'Forget', sourceActions, {type: 'button', hidden: ''});
  const crumbs = el('div', '', storage, {class: 'kdencut-breadcrumbs'});
  const up = el('button', '↑ Parent', crumbs, {type: 'button'});
  const folderLabel = el('span', '/', crumbs);
  const listing = el('div', '', storage, {class: 'kdencut-file-list', 'aria-label': 'Files'});
  const selectionBar = el('div', '', storage, {class: 'kdencut-selection-bar'});
  const importSelected = el('button', 'Add selected', selectionBar, {type: 'button', class: 'kdencut-primary', disabled: ''});
  const nas = el('form', '', content, {hidden: ''});
  el('h3', 'Add NAS connection', nas);
  el('p', 'Connect an SMB / Windows share. Downloads are saved locally on the server; originals stay on the NAS.', nas);
  const inputs = {};
  for (const [key, text, hint] of [['name', 'Name', 'My NAS'], ['server', 'NAS address', '192.168.50.10 or MNAS'],
      ['share', 'Share name', 'Video'], ['port', 'Port (optional)', '445'], ['username', 'Username (optional)', 'NAS username'], ['password', 'Password (optional)', 'NAS password']]) {
    const label = el('label', text, nas, {class: 'kdencut-field'});
    inputs[key] = el('input', '', label, {name: key, type: key === 'password' ? 'password' : key === 'port' ? 'number' : 'text', placeholder: hint,
      autocomplete: key === 'password' ? 'new-password' : 'off', autocapitalize: 'none', spellcheck: 'false'});
    if (key === 'server' || key === 'share') inputs[key].required = true;
    if (key === 'port') {inputs[key].min = 1; inputs[key].max = 65535;}
  }
  el('p', 'The connection and credentials are saved on this server.', nas, {class: 'kdencut-muted'});
  const nasSave = el('button', 'Connect and save', nas, {type: 'submit', class: 'kdencut-primary'});
  const nasBack = el('button', 'Back', nas, {type: 'button'});
  const footer = el('footer', '', panel);
  const status = el('p', '', footer, {role: 'status', 'aria-live': 'polite'});
  const progress = el('progress', '', footer, {max: '100', value: '0', hidden: ''});
  const serverPicker = el('button', 'Open server file picker', footer, {type: 'button', class: 'kdencut-muted'});
  let connections = new Set();

  async function api(route, body) {
    const response = await fetch(new URL(route, base), {method: body ? 'POST' : 'GET', cache: 'no-store',
      headers: {'X-KdenCut-Media': '1', ...(body ? {'Content-Type': 'application/json'} : {})},
      ...(body ? {body: JSON.stringify(body)} : {})});
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || 'Could not complete the request');
    return data;
  }
  function message(text, error = false) {
    status.textContent = text;
    status.classList.toggle('kdencut-error', error);
  }
  function lock(value) {
    busy = value;
    for (const button of panel.querySelectorAll('button, input, select')) button.disabled = value;
    close.disabled = false;
    importSelected.disabled = value || !selected.size;
    progress.hidden = !value;
  }
  function page(which) {
    device.hidden = which !== 'device'; storage.hidden = which !== 'storage'; nas.hidden = which !== 'nas';
    deviceTab.setAttribute('aria-pressed', String(which === 'device'));
    storageTab.setAttribute('aria-pressed', String(which !== 'device'));
    message('');
  }
  function hide() {
    request = null; browseSerial++; selected.clear();
    if (xhr) xhr.abort();
    xhr = null; lock(false); panel.close(); nas.reset();
    photoInput.value = ''; fileInput.value = '';
  }
  async function cancel(mode = 'cancel') {
    const id = request?.id;
    if (xhr) xhr.abort();
    if (!id) return hide();
    try { await api(mode, {request: id}); hide(); }
    catch (error) { message(error.message, true); }
  }
  close.onclick = () => cancel();
  panel.addEventListener('cancel', event => {event.preventDefault(); cancel();});
  serverPicker.onclick = () => cancel('server');
  deviceTab.onclick = () => page('device');
  storageTab.onclick = () => {page('storage'); loadSources();};
  addNas.onclick = () => page('nas');
  nasBack.onclick = () => page('storage');
  const bytes = size => size >= 1024**3 ? `${(size/1024**3).toFixed(1)} GB` : `${(size/1024**2).toFixed(1)} MB`;

  async function loadSources(preferred) {
    const currentId = request?.id;
    try {
      const data = await api('sources');
      if (request?.id !== currentId) return;
      sources.replaceChildren(); connections = new Set(data.connections.map(item => item.id));
      for (const item of [...data.roots, ...data.connections]) el('option', item.name, sources, {value: item.id});
      if (preferred) sources.value = preferred;
      source = sources.value; path = ''; selected.clear();
      forget.hidden = !connections.has(source);
      await browse();
    } catch (error) {message(error.message, true);}
  }
  sources.onchange = () => {source = sources.value; path = ''; selected.clear(); forget.hidden = !connections.has(source); browse();};
  up.onclick = () => {path = path.split('/').slice(0, -1).join('/'); selected.clear(); browse();};
  async function browse() {
    const serial = ++browseSerial;
    listing.replaceChildren(); folderLabel.textContent = '/' + path; up.disabled = !path;
    importSelected.disabled = true;
    importSelected.textContent = 'Add selected';
    if (!source) return message('Add a NAS connection to get started.');
    message('Loading folder…');
    try {
      const data = await api('list?' + new URLSearchParams({source, path}));
      if (serial !== browseSerial || !request) return;
      for (const entry of data.entries) {
        const itemPath = [path, entry.name].filter(Boolean).join('/');
        if (entry.directory) {
          const button = el('button', '▸ ' + entry.name, listing, {type: 'button', class: 'kdencut-directory'});
          button.onclick = () => {path = itemPath; selected.clear(); browse();};
        } else {
          const row = el('label', '', listing, {class: 'kdencut-file'});
          const check = el('input', '', row, {type: 'checkbox', 'aria-label': entry.name});
          const details = el('span', '', row, {class: 'kdencut-file-details'});
          el('span', entry.name, details); el('small', bytes(entry.size), details);
          check.onchange = () => {
            check.checked ? selected.add(itemPath) : selected.delete(itemPath);
            importSelected.disabled = !selected.size;
            importSelected.textContent = selected.size ? `Add selected (${selected.size})` : 'Add selected';
          };
        }
      }
      message(data.entries.length ? '' : 'No supported media in this folder.');
    } catch (error) {if (serial === browseSerial) message(error.message, true);}
  }
  nas.onsubmit = async event => {
    event.preventDefault(); const currentId = request?.id; lock(true); progress.removeAttribute('value'); message('Connecting to NAS…');
    try {
      const result = await api('connections', Object.fromEntries(Object.entries(inputs).map(([key, input]) => [key, input.value])));
      if (request?.id !== currentId) return;
      nas.reset(); page('storage'); await loadSources(result.id);
    } catch (error) {if (request?.id === currentId) message(error.message, true);}
    finally {if (request?.id === currentId) lock(false);}
  };
  forget.onclick = async () => {
    if (!confirm('Forget this NAS connection? Files already downloaded will stay on the server.')) return;
    try {await api('forget', {id: source}); await loadSources();} catch (error) {message(error.message, true);}
  };
  importSelected.onclick = async () => {
    const currentId = request?.id;
    lock(true); progress.removeAttribute('value'); message('Preparing selected media…');
    try {
      await api('select', {request: currentId, source, paths: [...selected]});
    } catch (error) {if (request?.id === currentId) {lock(false); message(error.message, true);}}
  };
  function sendFile(file, id, index, total) {
    return new Promise((resolve, reject) => {
      const transfer = new XMLHttpRequest(); xhr = transfer;
      transfer.open('PUT', new URL('upload?' + new URLSearchParams({request: id, name: file.name}), base));
      transfer.setRequestHeader('X-KdenCut-Media', '1');
      transfer.upload.onprogress = event => {
        if (request?.id !== id) return;
        progress.value = event.lengthComputable ? event.loaded/event.total*100 : 0;
        message(`Uploading ${index}/${total}: ${file.name} · ${bytes(event.loaded)}${event.lengthComputable ? ' / ' + bytes(event.total) : ''}`);
      };
      transfer.onload = () => {
        if (xhr === transfer) xhr = null;
        try {
          const data = JSON.parse(transfer.responseText);
          if (transfer.status < 200 || transfer.status >= 300) throw new Error(data.error || 'Upload failed');
          resolve(data.token);
        } catch (error) {reject(error);}
      };
      transfer.onerror = () => reject(new Error('Upload interrupted. Check your connection and choose the files again.'));
      transfer.onabort = () => reject(new Error('Upload cancelled'));
      transfer.send(file);
    });
  }
  async function upload(input) {
    const chosen = [...input.files]; input.value = '';
    if (!chosen.length || !request || busy) return;
    const id = request.id; const tokens = []; lock(true); progress.value = 0;
    try {
      for (const [index, file] of chosen.entries()) {
        if (request?.id !== id) return;
        tokens.push(await sendFile(file, id, index + 1, chosen.length));
      }
      if (request?.id === id) {message('Adding media to KdenCut…'); await api('finish', {request: id, files: tokens});}
    } catch (error) {
      if (request?.id === id) {lock(false); message(error.message, true);}
    }
  }
  photoInput.onchange = () => upload(photoInput);
  fileInput.onchange = () => upload(fileInput);
  async function poll() {
    try {
      const data = await api('status');
      if (!data.request || data.job.state === 'done') {if (request) hide(); return;}
      if (data.request.id !== request?.id) {
        if (request) hide();
        request = data.request; page('storage'); lock(false);
        heading.textContent = request.kind === 'audio' ? 'Add audio' : 'Add media';
        panel.showModal();
        loadSources();
      }
      if (data.job.state === 'copying') {
        lock(true);
        if (data.job.total) progress.value = data.job.done/data.job.total*100;
        else progress.removeAttribute('value');
        message(`Downloading ${data.job.file} · ${bytes(data.job.done || 0)} / ${bytes(data.job.total || 0)}`);
      } else if (data.job.state === 'error' && busy && !xhr) {
        lock(false); message(data.job.error, true);
      }
    } catch (_) {
      if (request && !xhr) message('Connection lost. Reconnecting to the server…', true);
    } finally {setTimeout(poll, document.hidden ? 2000 : 700);}
  }
  poll();
})();
