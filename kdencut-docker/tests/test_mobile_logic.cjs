// These tests exercise resize/reconnect behavior without a browser or server.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const test = require('node:test');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../root/usr/local/share/kdencut/mobile.js'), 'utf8');

function client(width, height, {touch = true, hash = ''} = {}) {
  const listeners = new Map(), timers = new Map(), messages = [], styles = new Map(), classes = new Set();
  let nextTimer = 0;
  const listen = (name, fn) => listeners.set(name, [...(listeners.get(name) || []), fn]);
  const viewport = {width, height, offsetTop: 0, offsetLeft: 0, addEventListener: listen};
  const pointer = {matches: touch, addEventListener: listen};
  const root = {classList: {toggle: (key, on) => on ? classes.add(key) : classes.delete(key)}, style: {setProperty: (k,v) => styles.set(k,v)}};
  const window = {innerWidth: width, innerHeight: height, visualViewport: viewport, matchMedia: () => pointer,
    addEventListener: listen, postMessage: value => messages.push(value)};
  const location = {origin: 'https://kdencut.test', hash};
  const document = {documentElement: root, body: {}, getElementById: () => ({}), querySelector: () => ({getBoundingClientRect: () => ({width: viewport.width, height: viewport.height - 56})})};
  vm.runInNewContext(source, {window, document, location, ResizeObserver: class {observe() {}},
    setTimeout: fn => {timers.set(++nextTimer, fn); return nextTimer;}, clearTimeout: id => timers.delete(id)});
  const flush = () => {for (const [id, fn] of timers) {timers.delete(id);fn();}};
  const emit = (type, value) => {for (const fn of listeners.get(type) || []) fn(value);flush();};
  const connect = (payload = {}) => emit('message', {source: window, origin: location.origin, data: {type: 'serverSettings', payload}});
  const resize = (w,h) => {viewport.width=w;viewport.height=h;window.innerWidth=w;window.innerHeight=h;emit('resize');};
  return {window, viewport, messages, classes, styles, emit, connect, resize};
}
const lastSize = c => c.messages.filter(m=>m.type === 'setManualResolution').at(-1);
const checkSize = c => {
  const size=lastSize(c);assert.ok(size.width>=960);assert.ok(size.height>=640);
  assert.equal(size.width%2,0);assert.equal(size.height%2,0);
  assert.ok(Math.abs(size.width/size.height-c.viewport.width/(c.viewport.height-56))<.01);
  assert.equal(c.messages.at(-1).settings.scaling_dpi,96);
};
test('phone portrait and landscape retain the full logical workspace',()=>{
  for(const [w,h] of [[320,568],[375,667],[430,932],[932,430],[1024,768]]){
    const c=client(w,h);c.connect();checkSize(c);c.resize(h,w);checkSize(c);
  }
});
test('keyboard viewport shrinks and restores without cropping',()=>{
  const c=client(430,932);c.connect();const initial={...lastSize(c)};
  c.viewport.height=400;c.emit('resize');checkSize(c);
  assert.equal(c.styles.get('--kdencut-bottom'),'532px');
  c.viewport.height=932;c.emit('resize');assert.deepEqual({...lastSize(c)},initial);
});
test('resize is idempotent but reconnect reapplies the fit',()=>{
  const c=client(430,932);c.connect();const n=c.messages.length;
  c.emit('resize');assert.equal(c.messages.length,n);
  c.connect();assert.equal(c.messages.length,n*2);
});
test('desktop and shared viewers do not change session settings',()=>{
  for (const options of [{touch:false},{hash:'#shared'},{hash:'#player2'}]) {
    const c=client(1440,900,options);c.connect();assert.equal(c.messages.length,0);assert.equal(c.classes.size,0);
  }
});
test('administrator resolution and DPI restrictions are respected',()=>{
  const c=client(430,932);c.connect({enable_resize:{value:false},scaling_dpi:{locked:true}});
  assert.equal(c.messages.length,1);assert.equal(c.messages[0].type,'setScaleLocally');
});
test('messages must come from this window and origin',()=>{
  const c=client(430,932);
  for(const source of [null,c.window]) c.emit('message',{source,origin:'https://elsewhere.test',data:{type:'serverSettings',payload:{}}});
  c.emit('message',{source:null,origin:'https://kdencut.test',data:{type:'serverSettings',payload:{}}});
  assert.equal(c.messages.length,0);
});
