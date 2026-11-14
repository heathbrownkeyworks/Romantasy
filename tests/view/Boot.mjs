// The page boots in a browser against the harness: the real bridge.js and app.js, then
// dev/fixture.js and dev/mock.js, as index.html?mock loads them. The boot completes and
// ends in ledger mode with the fixture's bonds (the harness once answered romantasyReady
// with UI mode before the mode, and the page asked again until the stack overflowed).
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

function element() {
    const attrs = {}, classes = new Set();
    return {
        innerHTML: '', className: '', textContent: '', value: '', style: {}, offsetWidth: 0, isConnected: true, disabled: false,
        classList: { add: c => classes.add(c), remove: c => classes.delete(c), toggle: (c, on) => (on ?? !classes.has(c)) ? classes.add(c) : classes.delete(c), contains: c => classes.has(c) },
        setAttribute: (k, v) => { attrs[k] = String(v); }, getAttribute: k => (k in attrs ? attrs[k] : null), removeAttribute: k => { delete attrs[k]; },
        querySelector: () => null, querySelectorAll: () => [], addEventListener() {}, focus() {}, appendChild() {}, remove() {}, insertAdjacentHTML() {}
    };
}
const elements = new Map();
const quiet = { log() {}, warn() {}, error: (...args) => { throw new Error('console.error during boot: ' + args.join(' ')); } };
const page = {
    console: quiet, setTimeout, clearTimeout, location: { search: '?mock' },
    document: {
        readyState: 'complete', documentElement: element(), head: element(),
        getElementById: id => { if (!elements.has(id)) elements.set(id, element()); return elements.get(id); },
        addEventListener() {}, createElement: element, querySelector: () => null, querySelectorAll: () => []
    },
    addEventListener() {}
};
page.window = page;
page.RM = { fit: { apply() {} } };
const run = path => vm.runInNewContext(readFileSync(new URL(path, import.meta.url), 'utf8'), page, { filename: path });
for (const file of ['bridge.js', 'strings.js', 'tiers.js', 'format.js', 'state.js', 'notice.js', 'roster.js', 'ledger.js', 'settings.js', 'modals.js', 'app.js']) run('../../view/js/' + file);
run('../../view/dev/fixture.js');
run('../../view/dev/mock.js');

const app = page.RM.app;
assert.equal(app.view.mode, 'ledger', 'the harness boots the page into the ledger');
assert.ok(app.state.romances.length > 0, 'with the fixture bonds');
assert.equal(page.RM.bridge.focused, true);
assert.equal(page.__romantasyMock.log.filter(entry => entry.name === 'romantasyReady').length, 1, 'romantasyReady is sent once');

console.log('Boot: ok');
