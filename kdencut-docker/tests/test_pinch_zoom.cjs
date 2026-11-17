const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../../src/timeline2/view/qml/Timeline.qml'), 'utf8');
function method(name) {
    const start = source.indexOf(`function ${name}(`);
    assert.ok(start >= 0);
    let braces = 0, body = source.indexOf('{', start);
    for (let i = body; i < source.length; i++) {
        if (source[i] === '{') braces++;
        if (source[i] === '}' && --braces === 0) return source.slice(start, i + 1);
    }
    throw new Error('Unclosed function');
}
test('pinch remains finite and bounded for empty, short and populated timelines', () => {
    for (const duration of [0, 1, 150, 90000]) for (const mobileLayout of [false, true]) {
        const root = {timeline: {duration, scaleFactor: 1}, timeScale: 1, width: 390, mobileLayout,
            clampContentX: x => Math.max(0, x), clampContentY: y => Math.max(0, y)};
        const context = {root, scrollView: {width: 390, contentX: 0, contentY: 0},
            K: {Core: {getCurrentFps: () => 30}}, active: true, startScale: 1,
            startContentY: 0, anchorFrame: 5, anchorX: 50, activeTranslation: {x: 10, y: 10}};
        vm.createContext(context);
        vm.runInContext(method('fitZoom') + '; root.fitZoom = fitZoom;' + method('applyPinch'), context);
        for (const factor of [0.001, 1, 1000]) {
            context.activeScale = factor;
            vm.runInContext('applyPinch()', context);
            assert.ok(Number.isFinite(root.timeline.scaleFactor));
            assert.ok(root.timeline.scaleFactor > 0 && root.timeline.scaleFactor <= 40);
            assert.ok(root.pinchTargetX === undefined || Number.isFinite(root.pinchTargetX));
        }
    }
});
