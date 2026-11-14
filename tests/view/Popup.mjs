// The level-change popup: the pure markup and identity rules, then its DOM flows with the
// real page scripts in a vm, a stand-in DOM, a fake clock and controllable animation frames.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const popup = createRequire(import.meta.url)('../../view/js/popup.js');

/* ---------- pure ---------- */
assert.equal(popup.AUTO_DISMISS_MS, 20000);

const gain = { followerName: 'Aelira', changeDirection: 'gain', previousLevelName: 'Lover', previousLevelIndex: 4, levelName: 'Spouse', levelIndex: 5, nextLevelName: '', points: 2500, pointsDelta: 60 };
const g = popup.popupHtml(gain);
for (const part of ['Romance Deepened', 'Aelira', 'icons/lover.png', 'icons/spouse.png', '+60', '2,500']) assert.ok(g.includes(part), 'gain shows ' + part);
assert.match(g, /class="[^"]*\bgain\b/);
assert.ok(!g.includes('Next:'), 'no Next line when nextLevelName is empty');

const loss = { followerName: 'Serana', changeDirection: 'loss', previousLevelName: 'Friend', previousLevelIndex: 3, levelName: 'Acquaintance', levelIndex: 2, nextLevelName: 'Friend', points: 990, pointsDelta: -150 };
const l = popup.popupHtml(loss);
for (const part of ['Romance Wounded', '−150', 'icons/heartbroken.png', 'Next: Friend']) assert.ok(l.includes(part), 'loss shows ' + part);
assert.match(l, /class="[^"]*\bloss\b/);
assert.ok(!l.includes('rays'), 'no rays on a wounded bond');
assert.ok(g.includes('rays'), 'rays behind a deepening bond');

const hostile = popup.popupHtml({ ...gain, followerName: '<i>x</i>' });
assert.ok(hostile.includes('&lt;i&gt;x&lt;/i&gt;') && !hostile.includes('<i>x</i>'), 'the name is escaped');
assert.equal(typeof popup.popupHtml({}), 'string');
assert.ok(popup.popupHtml({}).includes('data-action="popup.dismiss"'), 'an empty change can still be dismissed');

assert.equal(popup.same(gain, { ...gain }), true);
assert.equal(popup.same(gain, { ...gain, points: gain.points + 1 }), false);
assert.equal(popup.same(gain, null), false);
assert.equal(popup.same(null, gain), false);

// Icons: the tier name decides; the 1-based level number (RomanceManager's) stands in when the name is unknown.
assert.ok(popup.popupHtml({ ...gain, previousLevelName: '', levelName: '', previousLevelIndex: 1, levelIndex: 6 }).includes('icons/stranger.png'));
assert.ok(popup.popupHtml({ ...gain, previousLevelName: '', levelName: '', previousLevelIndex: 1, levelIndex: 6 }).includes('icons/spouse.png'));

// Unlock lines: under the new tier on a gain, as escaped text; never on a loss; absent as today.
const unlocked = popup.popupHtml({ ...gain, unlocks: ['A key to her house', '<b>x</b> & y', 7] });
assert.match(unlocked, />Unlocked</);
assert.ok(unlocked.includes('>A key to her house<'));
assert.ok(unlocked.includes('&lt;b&gt;x&lt;/b&gt; &amp; y') && !unlocked.includes('<b>x</b>'), 'an unlock line is escaped');
assert.ok(unlocked.indexOf('Unlocked') > unlocked.indexOf('class="shift"'), 'under the tier name');
assert.deepEqual(popup.normalize({ unlocks: ['a', 3, 'b'] }).unlocks, ['a', 'b']);
assert.deepEqual(popup.normalize({}).unlocks, []);
assert.doesNotMatch(popup.popupHtml({ ...loss, unlocks: ['A key to her house'] }), /Unlocked|A key to her house/, 'a loss lists none');
assert.equal(popup.popupHtml({ ...gain, unlocks: [] }), g, 'an empty list renders as today');
assert.doesNotMatch(g, /Unlocked/);

console.log('Popup (pure): ok');

/* ---------- flows: fake clock ---------- */
const plain = v => v === undefined || v === null ? v : JSON.parse(JSON.stringify(v));
let now = 0, nextTimer = 1;
const timers = new Map();
function setTimeoutFake(fn, ms) { const id = nextTimer++; timers.set(id, { at: now + (ms || 0), fn }); return id; }
function clearTimeoutFake(id) { timers.delete(id); }
function advance(ms) {
    const end = now + ms;
    for (;;) {
        let due = null;
        for (const [id, t] of timers) if (t.at <= end && (!due || t.at < due[1].at)) due = [id, t];
        if (!due) break;
        timers.delete(due[0]); now = due[1].at; due[1].fn();
    }
    now = end;
}
// Animation frames run only when the test says a frame was painted.
let frameQueue = [];
const requestAnimationFrame = fn => { frameQueue.push(fn); return frameQueue.length; };
function frame() { const due = frameQueue; frameQueue = []; for (const fn of due) fn(); }

/* ---------- stand-in DOM: a tree built from the markup, simple selectors only ---------- */
const doc = { activeElement: null };
const VOID = new Set(['img', 'input', 'br', 'path']);
const decode = v => v.replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&');
function element(tag) {
    const attrs = {}, listeners = {};
    let html = '';
    const el = {
        tagName: (tag || 'div').toUpperCase(), parent: null, children: [], attrs, listeners, style: {},
        offsetWidth: 0, isConnected: true, disabled: false, value: '',
        get className() { return attrs.class || ''; },
        set className(v) { attrs.class = String(v); },
        classList: {
            add: c => { const s = new Set(el.className.split(/\s+/).filter(Boolean)); s.add(c); el.className = [...s].join(' '); },
            remove: c => { el.className = el.className.split(/\s+/).filter(x => x && x !== c).join(' '); },
            contains: c => el.className.split(/\s+/).includes(c),
            toggle: (c, on) => ((on ?? !el.classList.contains(c)) ? el.classList.add(c) : el.classList.remove(c))
        },
        get innerHTML() { return html; },
        set innerHTML(v) {
            html = String(v);
            for (const c of el.children) detach(c);
            el.children = [];
            const stack = [el];
            for (const m of html.matchAll(/<(\/?)([a-zA-Z0-9]+)((?:\s+[\w-]+(?:="[^"]*")?)*)\s*(\/?)>/g)) {
                if (m[1]) { if (stack.length > 1) stack.pop(); continue; }
                const child = element(m[2].toLowerCase());
                for (const a of m[3].matchAll(/([\w-]+)(?:="([^"]*)")?/g)) child.attrs[a[1]] = a[2] === undefined ? '' : decode(a[2]);
                child.parent = stack[stack.length - 1];
                child.parent.children.push(child);
                if (!m[4] && !VOID.has(child.tagName.toLowerCase())) stack.push(child);
            }
        },
        setAttribute: (k, v) => { attrs[k] = String(v); }, getAttribute: k => (k in attrs ? attrs[k] : null), removeAttribute: k => { delete attrs[k]; },
        matches: selector => matches(el, selector),
        querySelectorAll: selector => { const out = []; walk(el, n => { if (matches(n, selector)) out.push(n); }); return out; },
        querySelector: selector => el.querySelectorAll(selector)[0] || null,
        closest: selector => { for (let n = el; n; n = n.parent) if (n.matches && matches(n, selector)) return n; return null; },
        contains: other => { for (let n = other; n; n = n.parent) if (n === el) return true; return false; },
        addEventListener: (type, fn) => { listeners[type] = fn; },
        focus() { doc.activeElement = el; },
        remove() {}, appendChild() {}, insertAdjacentHTML() {}
    };
    return el;
}
function detach(n) { n.isConnected = false; if (doc.activeElement === n) doc.activeElement = null; for (const c of n.children) detach(c); }
function walk(root, fn) { for (const c of root.children) { fn(c); walk(c, fn); } }
function matches(el, selector) {
    return selector.split(',').some(sel => {
        const m = sel.trim().match(/^([a-z0-9]+)?((?:\.[\w-]+|\[[\w-]+(?:="[^"]*")?\])*)$/);
        if (!m) throw new Error('stand-in DOM cannot match ' + sel);
        if (m[1] && el.tagName.toLowerCase() !== m[1]) return false;
        for (const p of m[2].matchAll(/\.([\w-]+)|\[([\w-]+)(?:="([^"]*)")?\]/g)) {
            if (p[1] && !el.classList.contains(p[1])) return false;
            if (p[2] && (el.getAttribute(p[2]) === null || (p[3] !== undefined && el.getAttribute(p[2]) !== p[3]))) return false;
        }
        return true;
    });
}

const fixtureBox = { window: {} };
vm.runInNewContext(readFileSync(new URL('../../view/dev/fixture.js', import.meta.url), 'utf8'), fixtureBox);
const FIXTURE = fixtureBox.window.ROMANTASY_FIXTURE;

const elements = new Map();
const sent = [];
let sendResult = true;
const topics = new Map();
const root = element('html');
const page = {
    console, setTimeout: setTimeoutFake, clearTimeout: clearTimeoutFake, requestAnimationFrame,
    document: {
        readyState: 'complete', documentElement: root,
        get activeElement() { return doc.activeElement; },
        getElementById: id => { if (!elements.has(id)) elements.set(id, element()); return elements.get(id); },
        querySelector: () => null, addEventListener() {}, createElement: element
    }
};
page.window = page;
page.RM = {
    fit: { apply() {} },
    bridge: {
        focused: true,
        on: (topic, fn) => topics.set(topic, fn),
        send: (name, payload) => { sent.push([name, plain(payload)]); return sendResult; },
        sound() {}
    }
};
for (const file of ['strings.js', 'tiers.js', 'format.js', 'state.js', 'notice.js', 'enrollment.js', 'roster.js', 'ledger.js', 'settings.js', 'modals.js', 'popup.js', 'app.js']) {
    vm.runInNewContext(readFileSync(new URL('../../view/js/' + file, import.meta.url), 'utf8'), page);
}
const RM = page.RM, app = RM.app;
const host = page.document.getElementById('popup');
const stage = page.document.getElementById('app');

let revision = 1;
const publishState = () => topics.get('romantasyState')({ ...plain(FIXTURE), revision: ++revision, message: '' });
const levelChange = payload => topics.get('romantasyLevelChange')(payload);
const mode = m => topics.get('romantasyMode')({ mode: m });
const uimode = on => { RM.bridge.focused = on; topics.get('__uimode')(on); };
// A click as the browser delivers it: app.js's own listener on #app, with the target that was hit.
const click = target => stage.listeners.click({ target });
const button = () => host.querySelector('[data-action="popup.dismiss"].btn');
const scrim = () => host.querySelector('.scrim');
const of = name => sent.filter(s => s[0] === name).map(s => s[1]);
const shown = () => of('romantasyPopupShown');
const dismissed = () => of('romantasyPopupDismissed');
const change = (seq, extra) => ({ ...gain, seq, ...extra });
const settle = () => { frame(); frame(); advance(5000); };

publishState();
uimode(true);
mode('ledger');
assert.equal(typeof stage.listeners.click, 'function');

/* ---------- Shown: once, after two painted frames, with the popup's seq ---------- */
levelChange(change(1));
assert.ok(host.classList.contains('open'), '#popup opens');
assert.ok(host.innerHTML.includes('Romance Deepened'));
assert.equal(app.view.popup.seq, 1);
assert.equal(doc.activeElement, button(), 'the button takes focus');
assert.deepEqual(shown(), [], 'nothing before the first frame');
frame();
assert.deepEqual(shown(), [], 'nothing after one frame');
frame();
assert.deepEqual(shown(), [{ seq: 1 }], 'Shown after the second frame');
frame(); frame();
assert.deepEqual(shown(), [{ seq: 1 }], 'and only once');

/* ---------- the same popup again: nothing sent, the fuse keeps running ---------- */
advance(12000);
levelChange(change(1));
const keptMarkup = host.innerHTML;
frame(); frame();
assert.deepEqual(shown(), [{ seq: 1 }], 'a re-published popup sends nothing');
assert.equal(host.innerHTML, keptMarkup, 'and is not repainted');

/* ---------- the fuse: dismisses at 20 000 ms, Dismissed once ---------- */
advance(20000 - 12000 - 1);
assert.ok(!host.classList.contains('closing'), 'still up at 19 999 ms');
advance(1);
assert.ok(host.classList.contains('closing'), 'closing at 20 000 ms');
assert.deepEqual(dismissed(), [], 'Dismissed waits for the closing motion');
advance(219);
assert.deepEqual(dismissed(), []);
advance(1);
assert.deepEqual(dismissed(), [{ seq: 1 }]);
assert.equal(app.view.popup, null);
assert.ok(!host.classList.contains('open') && !host.classList.contains('closing'));
assert.equal(host.innerHTML, '');
advance(60000);
assert.deepEqual(dismissed(), [{ seq: 1 }], 'once');

/* ---------- a double click on the button: Dismissed once ---------- */
sent.length = 0;
levelChange(change(2));
frame(); frame();
const twice = button();
click(twice);
click(twice);
advance(100);
click(twice);
advance(500);
assert.deepEqual(dismissed(), [{ seq: 2 }]);
assert.deepEqual(shown(), [{ seq: 2 }]);

/* ---------- the scrim dismisses; the card does not ---------- */
sent.length = 0;
levelChange(change(3, { followerName: 'Inalion' }));
frame(); frame();
click(host.querySelector('h2'));
click(host.querySelector('.sheet'));
advance(500);
assert.ok(host.classList.contains('open') && !host.classList.contains('closing'), 'a click inside the card leaves it up');
assert.deepEqual(dismissed(), []);
click(scrim());
advance(220);
assert.deepEqual(dismissed(), [{ seq: 3 }]);

/* ---------- a different popup replaces the one showing; nothing is sent for the old one ---------- */
sent.length = 0;
levelChange(change(4));
frame(); frame();
levelChange(change(5, { followerName: 'Serana', changeDirection: 'loss', pointsDelta: -40, points: 1990 }));
assert.equal(app.view.popup.seq, 5);
assert.ok(host.innerHTML.includes('Romance Wounded'));
frame(); frame();
assert.deepEqual(shown(), [{ seq: 4 }, { seq: 5 }]);
assert.deepEqual(dismissed(), [], 'C++ already let the old head go');
// replaced before its Shown went out: the old one never reports Shown
levelChange(change(6));
frame();
levelChange(change(7, { points: 2600 }));
frame(); frame();
assert.deepEqual(shown(), [{ seq: 4 }, { seq: 5 }, { seq: 7 }]);
// the replacement's fuse is its own: 20 s from its arrival
advance(19999);
assert.deepEqual(dismissed(), []);
advance(221);
assert.deepEqual(dismissed(), [{ seq: 7 }]);

/* ---------- dismissed before Shown went out: no Shown, Dismissed still sent ---------- */
sent.length = 0;
levelChange(change(8));
click(button());
frame(); frame();
assert.deepEqual(shown(), [], 'a closing popup does not report Shown');
advance(220);
assert.deepEqual(dismissed(), [{ seq: 8 }], 'C++ drops a Requested head on Dismissed instead of waiting out its timeout');
frame(); frame();
assert.deepEqual(shown(), []);

/* ---------- a dismissed popup re-published late is not shown again ---------- */
levelChange(change(8));
assert.equal(app.view.popup, null);
assert.ok(!host.classList.contains('open'));

/* ---------- popup mode -> ledger mode keeps the popup and its fuse ---------- */
sent.length = 0;
mode('popup');
assert.equal(root.getAttribute('data-mode'), 'popup');
levelChange(change(9));
frame(); frame();
advance(15000);
const before = app.view.popup;
mode('ledger');                 // C++ retargets the view in place...
levelChange(change(9));         // ...and re-publishes the head
publishState();
frame(); frame();
assert.equal(app.view.popup, before, 'the same popup');
assert.ok(host.classList.contains('open') && !host.classList.contains('closing'));
assert.deepEqual(shown(), [{ seq: 9 }]);
advance(4999);
assert.ok(!host.classList.contains('closing'), 'the fuse was not restarted');
advance(1 + 220);
assert.deepEqual(dismissed(), [{ seq: 9 }]);

/* ---------- __uimode: a blink keeps the popup; 250 ms off clears it silently ---------- */
sent.length = 0;
levelChange(change(10));
frame(); frame();
uimode(false);
advance(200);
uimode(true);
assert.equal(app.view.mode, 'ledger', 'the blink restores the mode');
assert.equal(app.view.popup && app.view.popup.seq, 10, 'a blink keeps the popup');
advance(1000);
assert.ok(host.classList.contains('open') && !host.classList.contains('closing'));
uimode(false);
advance(249);
assert.equal(app.view.popup && app.view.popup.seq, 10);
advance(1);
assert.equal(app.view.popup, null, 'cleared after 250 ms off');
advance(220);
assert.ok(!host.classList.contains('open'));
advance(60000);
assert.deepEqual(dismissed(), [], 'cleared without a message, and the fuse is gone');
assert.deepEqual(shown(), [{ seq: 10 }]);
uimode(true);
mode('ledger');

/* ---------- a romantasyMode message ends a pending release ---------- */
// The ledger closes, a popup enters inside 250 ms, then the host's late __uimode true lands.
sent.length = 0;
uimode(false);
advance(100);
mode('popup');
levelChange(change(11));
uimode(true);
assert.equal(app.view.mode, 'popup', 'the popup is not turned into the ledger');
advance(1000);
assert.equal(app.view.mode, 'popup');
assert.equal(app.view.popup && app.view.popup.seq, 11, 'nor dropped by the old release');
click(button());
advance(220);
assert.deepEqual(dismissed(), [{ seq: 11 }]);
mode('ledger');
// The same, with the popup's __uimode true landing later than 250 ms after the close:
// the old release must not fire and drop the new popup.
sent.length = 0;
uimode(false);
advance(100);
mode('popup');
levelChange(change(14));
frame(); frame();
advance(300);
assert.equal(app.view.mode, 'popup');
assert.equal(app.view.popup && app.view.popup.seq, 14, 'the old release does not drop the new popup');
uimode(true);
assert.equal(app.view.mode, 'popup');
click(button());
advance(220);
assert.deepEqual(shown(), [{ seq: 14 }]);
assert.deepEqual(dismissed(), [{ seq: 14 }]);
mode('ledger');

/* ---------- a popup taking over a just-closed ledger drops the ledger's open sheets ---------- */
// The ledger closes with a takeover, a modal or the settings pane open; a popup enters inside 250 ms.
const takeoverEl = page.document.getElementById('takeover'), modalEl = page.document.getElementById('modal'), paneEl = page.document.getElementById('pane');
[['takeover', () => app.emit('open-enroll', { mode: 'create' })],
    ['modal', () => app.emit('confirm', { kind: 'reset', identity: 'FE964801' })],
    ['settings', () => app.emit('open-settings')]].forEach(([what, openIt], i) => {
    sent.length = 0;
    openIt();
    if (what === 'takeover') { assert.ok(app.view.takeover && root.classList.contains('enrolling') && takeoverEl.classList.contains('open')); }
    if (what === 'modal') { assert.ok(app.view.modal && modalEl.classList.contains('open')); }
    if (what === 'settings') { assert.equal(app.view.pane, 'settings'); assert.ok(paneEl.className.includes('settings')); }
    uimode(false);
    advance(100);
    mode('popup');
    levelChange(change(30 + i));
    frame(); frame();
    assert.equal(app.view.takeover, null, what + ': takeover cleared');
    assert.equal(app.view.modal, null, what + ': modal cleared');
    assert.equal(app.view.pane, 'dossier', what + ': pane back to the dossier');
    assert.ok(!root.classList.contains('enrolling'), what + ': html.enrolling gone');
    assert.equal(app.view.popup && app.view.popup.seq, 30 + i, what + ': the popup is kept');
    uimode(true);
    advance(10000);
    mode('ledger');                  // the player opens the ledger over the popup
    levelChange(change(30 + i));     // C++ re-publishes the head
    publishState();
    advance(220);
    assert.ok(!takeoverEl.classList.contains('open') && takeoverEl.innerHTML === '', what + ': no stale takeover painted');
    assert.ok(!modalEl.classList.contains('open'), what + ': no stale modal painted');
    assert.ok(!paneEl.className.includes('settings'), what + ': no stale settings pane painted');
    assert.equal(app.view.popup && app.view.popup.seq, 30 + i, what + ': ledger mode keeps the popup');
    advance(10000 - 220 - 1);
    assert.ok(!host.classList.contains('closing'), what + ': the fuse was not restarted');
    advance(1 + 220);
    assert.deepEqual(shown(), [{ seq: 30 + i }]);
    assert.deepEqual(dismissed(), [{ seq: 30 + i }]);
});

/* ---------- mode "hidden": the plugin left; the popup goes at once, silently ---------- */
// After a refused entry no __uimode comes: "hidden" is the page's only word that it is off screen.
sent.length = 0;
mode('popup');
levelChange(change(40));
frame(); frame();
assert.deepEqual(shown(), [{ seq: 40 }]);
advance(5000);
mode('hidden');
assert.equal(app.view.mode, 'hidden');
assert.ok(root.classList.contains('asleep'), 'asleep at once');
assert.equal(app.view.popup, null, 'the popup is cleared at once');
advance(220);
assert.ok(!host.classList.contains('open'), 'and taken off');
advance(60000);
assert.deepEqual(dismissed(), [], 'nothing sent for it, and its fuse is gone');
assert.deepEqual(shown(), [{ seq: 40 }]);
// The plugin tries the same head again: shown again, Shown sent again (C++ acts on Shown only for a Requested head).
mode('popup');
levelChange(change(40));
assert.equal(app.view.popup && app.view.popup.seq, 40, 'the same seq is shown again');
assert.ok(host.classList.contains('open'));
frame(); frame();
assert.deepEqual(shown(), [{ seq: 40 }, { seq: 40 }], 'and reports Shown again');
// hidden while its closing motion plays: no Dismissed either (C++ has already let it go).
click(button());
advance(100);
mode('hidden');
advance(1000);
assert.deepEqual(dismissed(), [], 'a dismissal cut short by hidden sends nothing');
// hidden before its Shown went out: no Shown later, by frame or by timer.
mode('popup');
levelChange(change(41));
frame();
mode('hidden');
frame(); frame(); advance(1000);
assert.deepEqual(shown(), [{ seq: 40 }, { seq: 40 }], 'a popup dropped before Shown never reports it');
// hidden ends a pending release: the release timer does not run after it.
mode('ledger');
uimode(false);
mode('hidden');
uimode(true);
assert.equal(app.view.mode, 'hidden', 'a later __uimode on does not restore the mode hidden replaced');
mode('ledger');

// hidden over the ledger drops its open sheets at once, as a completed release does.
[['takeover', () => app.emit('open-enroll', { mode: 'create' })],
    ['modal', () => app.emit('confirm', { kind: 'reset', identity: 'FE964801' })],
    ['settings', () => app.emit('open-settings')]].forEach(([what, openIt]) => {
    sent.length = 0;
    openIt();
    assert.ok(app.view.takeover || app.view.modal || app.view.pane === 'settings', what + ' is open');
    mode('hidden');
    assert.equal(app.view.takeover, null, what + ': takeover cleared');
    assert.equal(app.view.modal, null, what + ': modal cleared');
    assert.equal(app.view.pane, 'dossier', what + ': pane back to the dossier');
    assert.ok(!root.classList.contains('enrolling'), what + ': html.enrolling gone');
    advance(300);
    assert.ok(!takeoverEl.classList.contains('open') && !modalEl.classList.contains('open') && !paneEl.className.includes('settings'), what + ': nothing left painted');
    assert.deepEqual(sent, [], what + ': nothing sent');
    mode('ledger');
    publishState();
    assert.ok(!app.view.takeover && !app.view.modal && app.view.pane === 'dossier', what + ': the next open starts clean');
});

/* ---------- Shown without animation frames: the timer reports it, once ---------- */
sent.length = 0;
levelChange(change(42));
advance(popup.SHOWN_FALLBACK_MS - 1);
assert.deepEqual(shown(), [], 'not before the fallback');
advance(1);
assert.deepEqual(shown(), [{ seq: 42 }], 'the fallback reports Shown when no frame ran');
frame(); frame(); frame();
assert.deepEqual(shown(), [{ seq: 42 }], 'late frames do not report it twice');
advance(20220);
// Frames first: the timer then sends nothing.
sent.length = 0;
levelChange(change(43));
frame(); frame();
advance(1000);
assert.deepEqual(shown(), [{ seq: 43 }], 'frames first, the timer adds nothing');
// A closing popup reports nothing by timer either.
click(button());
advance(220);
levelChange(change(44));
click(button());
advance(1000);
assert.deepEqual(shown(), [{ seq: 43 }], 'a popup dismissed inside the fallback never reports Shown');
assert.deepEqual(dismissed(), [{ seq: 43 }, { seq: 44 }]);

/* ---------- hostile strings in every field are escaped ---------- */
const evil = '<img src=x onerror=alert(1)>"\'&';
const evilChange = {};
for (const key of ['followerName', 'changeDirection', 'previousLevelName', 'previousLevelIndex', 'levelName', 'levelIndex', 'nextLevelName', 'points', 'pointsDelta', 'seq']) evilChange[key] = evil;
for (const html of [popup.popupHtml(evilChange), popup.popupHtml({ ...gain, followerName: evil, previousLevelName: evil, levelName: evil, nextLevelName: evil })]) {
    assert.ok(!html.includes('<img src=x'), 'no raw markup');
    assert.ok(!html.includes('"\''), 'no raw quotes');
}
sent.length = 0;
levelChange(evilChange);
assert.ok(host.innerHTML.includes('&lt;img src=x onerror=alert(1)&gt;'), 'the name shows, escaped');
assert.ok(!host.querySelectorAll('img').some(img => img.getAttribute('src') === 'x'));
frame(); frame();
assert.deepEqual(shown(), [{ seq: 0 }], 'an unusable seq echoes 0');
click(button());
advance(220);
assert.deepEqual(dismissed(), [{ seq: 0 }]);

/* ---------- malformed payloads never throw and stay dismissible ---------- */
const odd = [
    {}, { followerName: 'Aelira' }, { changeDirection: 'sideways', levelName: 'Lover' },
    { followerName: 5, levelName: ['Lover'], levelIndex: '5', previousLevelIndex: 99, points: '2,500', pointsDelta: NaN, nextLevelName: {} },
    { points: Infinity, pointsDelta: -Infinity, seq: -3 }, { seq: 1.5 }, { seq: '12' }, { followerName: null, changeDirection: null }
];
for (const payload of odd) {
    sent.length = 0;
    assert.doesNotThrow(() => levelChange(payload));
    assert.ok(host.classList.contains('open'), 'shown: ' + JSON.stringify(payload));
    assert.ok(button(), 'with its button');
    frame(); frame();
    assert.deepEqual(shown(), [{ seq: 0 }]);
    click(button());
    advance(220);
    assert.deepEqual(dismissed(), [{ seq: 0 }]);
    assert.ok(!host.classList.contains('open'));
}
const wrongDirection = popup.popupHtml({ ...gain, changeDirection: 'sideways' });
assert.ok(wrongDirection.includes('Romance Deepened'), 'an unknown direction reads as a gain, as C++ plays it');
// Not an object at all (a payload that did not parse): nothing to show, nothing sent.
for (const payload of [null, undefined, 'x', 42, true, []]) {
    sent.length = 0;
    assert.doesNotThrow(() => levelChange(payload));
    assert.equal(app.view.popup, null);
    frame(); frame(); advance(30000);
    assert.deepEqual(sent, []);
}

/* ---------- a failed send leaves nothing stuck ---------- */
sent.length = 0;
sendResult = false;
levelChange(change(12));
frame(); frame();
click(button());
advance(220);
assert.equal(app.view.popup, null, 'dismissed although the host did not take the message');
assert.ok(!host.classList.contains('open'));
levelChange(change(13));
assert.ok(host.classList.contains('open'), 'the next popup still shows');
advance(20220);
assert.equal(app.view.popup, null, 'and its fuse still runs');
sendResult = true;

console.log('Popup: ok');
