// Keys and the gamepad through the real page scripts: pad.js's one keydown listener on document,
// a stand-in Magelight gamepad helper, a stand-in DOM, a fake clock and controllable frames.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const fixtureBox = { window: {} };
vm.runInNewContext(readFileSync(new URL('../../view/dev/fixture.js', import.meta.url), 'utf8'), fixtureBox);
const FIXTURE = fixtureBox.window.ROMANTASY_FIXTURE;
const plain = v => v === undefined || v === null ? v : JSON.parse(JSON.stringify(v));

/* ---------- fake clock and frames ---------- */
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
let frameQueue = [];
const requestAnimationFrame = fn => { frameQueue.push(fn); return frameQueue.length; };
const frames = () => { for (let i = 0; i < 3; i++) { const due = frameQueue; frameQueue = []; for (const fn of due) fn(); } };

/* ---------- stand-in DOM: a tree built from the markup, simple selectors only ---------- */
const doc = { activeElement: null };
const VOID = new Set(['img', 'input', 'br', 'path']);
const decode = v => v.replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&');
// Layout, for the tests that need it: layout(el) gives { top, height, scrollHeight } in stage pixels (top relative to
// the viewport, so a scroller's children move up by its scrollTop), or null for nothing laid out. Sizes set on an
// element by a test win over it. overflowOf(el) is what getComputedStyle reports for overflow-y.
let layout = () => null;
let overflowOf = el => el.classList.contains('scroller') ? 'auto' : 'visible';
function element(tag) {
    const attrs = {}, listeners = {}, own = {};
    let html = '';
    const size = name => ({
        get() { return name in own ? own[name] : ((layout(el) || {})[name === 'scrollHeight' ? 'scrollHeight' : 'height'] || 0); },
        set(v) { own[name] = v; }, enumerable: true
    });
    const el = {
        tagName: (tag || 'div').toUpperCase(), parent: null, children: [], attrs, listeners, style: {},
        scrollTop: 0, offsetWidth: 0, offsetTop: 0, isConnected: true, disabled: false, value: '', scrolled: 0,
        get parentElement() { return el.parent; },
        getBoundingClientRect() { const b = layout(el) || {}, top = b.top || 0, height = b.height || 0; return { top, height, bottom: top + height, left: 0, width: 0, right: 0 }; },
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
                if ('value' in child.attrs) child.value = child.attrs.value;
                child.disabled = 'disabled' in child.attrs;
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
        focus() { doc.activeElement = el; if (docListeners.focusin) docListeners.focusin({ target: el }); },
        blur() { if (doc.activeElement === el) doc.activeElement = null; },
        scrollIntoView() { el.scrolled++; },
        remove() {}, appendChild() {}, insertAdjacentHTML() {}
    };
    for (const name of ['clientHeight', 'offsetHeight', 'scrollHeight']) Object.defineProperty(el, name, size(name));
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

/* ---------- a stand-in for Magelight's gamepad helper: records the scopes the page attaches ---------- */
const scopes = [];
const helper = {
    attachNavigation(options) { const s = { options, open: true }; scopes.push(s); return () => { s.open = false; }; },
    onStateChange() { return () => {}; },
    getPrompt: action => ({ action, label: { up: 'D-pad Up', down: 'D-pad Down', accept: 'A', cancel: 'B' }[action] || '' })
};
const openScopes = () => scopes.filter(s => s.open);
const topScope = () => openScopes()[openScopes().length - 1];

/* ---------- the page ---------- */
const elements = new Map();
const sent = [], sounds = [];
const topics = new Map();
const docListeners = {};
const page = {
    console, setTimeout: setTimeoutFake, clearTimeout: clearTimeoutFake, requestAnimationFrame,
    getComputedStyle: el => ({ overflowY: overflowOf(el) }),
    document: {
        readyState: 'complete', documentElement: element('html'),
        get activeElement() { return doc.activeElement; },
        getElementById: id => { if (!elements.has(id)) elements.set(id, element()); return elements.get(id); },
        querySelector: () => null, createElement: element,
        addEventListener: (type, fn, capture) => {
            assert.equal(docListeners[type], undefined, 'one ' + type + ' listener on document');
            docListeners[type] = fn;
            docCapture[type] = capture === true;
        }
    }
};
const docCapture = {};
page.window = page;
const winListeners = {};
page.addEventListener = (type, fn, capture) => { assert.equal(capture, true, type + ' is caught in the capture phase'); winListeners[type] = fn; };
page.RM = {
    fit: { apply() {} },
    bridge: {
        focused: true,
        on: (topic, fn) => topics.set(topic, fn),
        send: (name, payload) => { sent.push([name, plain(payload)]); return true; },
        sound: name => sounds.push(name),
        gamepad: () => helper
    }
};
for (const file of ['strings.js', 'tiers.js', 'format.js', 'state.js', 'notice.js', 'enrollment.js', 'roster.js', 'ledger.js', 'settings.js', 'journal.js', 'modals.js', 'popup.js', 'pad.js', 'app.js']) {
    vm.runInNewContext(readFileSync(new URL('../../view/js/' + file, import.meta.url), 'utf8'), page);
}
const RM = page.RM, app = RM.app, byId = page.document.getElementById;
const roster = byId('roster'), pane = byId('pane'), takeover = byId('takeover'), modal = byId('modal'), popup = byId('popup'), legend = byId('legend');
let revision = 1;
const publish = extra => topics.get('romantasyState')({ ...plain(FIXTURE), revision: ++revision, message: '', ...extra });
const mode = m => topics.get('romantasyMode')({ mode: m });
let seq = 0;
const levelChange = () => topics.get('romantasyLevelChange')({ followerName: 'Aelira', changeDirection: 'gain', levelName: 'Lover', points: 2010 + seq, seq: ++seq });
const keydown = docListeners.keydown;
assert.equal(typeof keydown, 'function', 'pad.js listens on document');
function key(k, o = {}) {
    const ev = { key: k, code: o.code || '', ctrlKey: !!o.ctrl, altKey: false, shiftKey: !!o.shift, metaKey: false, repeat: !!o.repeat, isComposing: !!o.composing,
        target: doc.activeElement, prevented: 0, preventDefault() { ev.prevented++; } };
    keydown(ev);
    return ev;
}
const chord = o => key('r', Object.assign({ code: 'KeyR', ctrl: true }, o));
// A whole key press as Magelight fires it: RawKeyDown, then Char (keypress), then KeyUp, each its own event. Where
// the page does not cancel them, WebKit's own button behaviour runs on whatever has focus by then: a focused
// button clicks itself on keypress Enter and on keyup Space.
function press(k) {
    const down = key(k);
    const other = type => { const ev = { type, key: k, keyCode: k === 'Enter' ? 13 : 32, prevented: 0, stopped: 0,
        preventDefault() { ev.prevented++; }, stopImmediatePropagation() { ev.stopped++; } }; winListeners[type](ev); return ev; };
    const native = ev => { const el = doc.activeElement; if (!ev.prevented && !ev.stopped && el && el.tagName === 'BUTTON') click(el); };
    const keypress = other('keypress');
    if (k === 'Enter') native(keypress);
    const keyup = other('keyup');
    if (k === ' ') native(keyup);
    return { down, keypress, keyup };
}
// A click as the browser delivers it: app.js's listener on #app, with the target that was hit.
const click = target => byId('app').listeners.click({ target });
const actionIn = (host, action) => host.querySelector('[data-action="' + action + '"]');
const of = name => sent.filter(s => s[0] === name).map(s => s[1]);
const rows = () => RM.roster.shownRows(app.state, app.view).map(RM.state.rowIdentity);
const rowEl = id => roster.querySelectorAll('[data-identity]').find(el => el.getAttribute('data-identity') === id);

publish();
mode('ledger');
advance(2000);
assert.ok(legend.innerHTML.includes('Select') && legend.innerHTML.includes('Close'), 'the ledger legend shows');
assert.equal(typeof winListeners.keypress, 'function');
assert.equal(typeof winListeners.keyup, 'function');
// The ledger opens on its selected row, so Enter and the first Down start from it.
assert.equal(doc.activeElement, rowEl(rows()[0]), 'the ledger opens focused on the selected row');

/* ---------- Up and Down walk the visible rows and stop at either end ---------- */
const ids = rows();
assert.ok(ids.length >= 3, 'the fixture has rows to walk');
assert.equal(app.view.selected, ids[0]);
sent.length = 0;
// The roster's rows list is 200 tall; rows are 70 tall every 78 (the group headings take no room here).
const rowsList = () => roster.querySelector('.rows');
layout = el => {
    if (el.classList.contains('rows')) return { top: 0, height: 200 };
    if (el.classList.contains('row') && el.parent && el.parent.classList.contains('rows')) {
        return { top: 78 * el.parent.children.filter(c => c.classList.contains('row')).indexOf(el) - el.parent.scrollTop, height: 70 };
    }
    return null;
};
const pageScroll = () => [byId('app'), pane, roster, page.document.documentElement].map(el => el.scrollTop || 0);
const down = key('ArrowDown');
assert.equal(down.prevented, 1);
assert.equal(app.view.selected, ids[1]);
assert.equal(doc.activeElement, rowEl(ids[1]), 'the new row takes focus');
assert.equal(rowsList().scrollTop, 0, 'a row already in view moves nothing');
for (let i = 0; i < ids.length + 2; i++) key('ArrowDown', { repeat: true });
assert.equal(app.view.selected, ids[ids.length - 1], 'held Down stops at the last row');
assert.equal(rowsList().scrollTop, 78 * (ids.length - 1) + 70 - 200, 'the rows list scrolls just enough to show the last row');
assert.deepEqual(pageScroll(), [0, 0, 0, 0], 'and nothing above it scrolls');
key('ArrowDown');
assert.equal(app.view.selected, ids[ids.length - 1]);
for (let i = 0; i < ids.length + 2; i++) key('ArrowUp');
assert.equal(app.view.selected, ids[0], 'Up stops at the first row');
assert.equal(rowsList().scrollTop, 0, 'and the list is back at its top');
layout = () => null;
assert.deepEqual(sent, [], 'walking the roster sends nothing');
// Away rows hidden: the walk covers only what is shown.
publish({ showAwayFollowers: false });
const shown = rows();
assert.ok(shown.length < ids.length);
for (let i = 0; i < ids.length + 2; i++) key('ArrowDown');
assert.equal(app.view.selected, shown[shown.length - 1]);
publish();
for (let i = 0; i < ids.length + 2; i++) key('ArrowUp');
// Enter on a focused row opens it, as its click does (the click sound with it).
rowEl(ids[2]).focus();
sounds.length = 0;
key('Enter');
assert.equal(app.view.selected, ids[2]);
assert.deepEqual(sounds, ['click']);
key('ArrowUp');
key('ArrowUp');
assert.equal(app.view.selected, ids[0]);

/* ---------- arrows reach no row while a takeover, a modal or a popup is open ---------- */
app.emit('open-enroll', { mode: 'create' });
key('ArrowDown'); key('ArrowUp'); key('ArrowDown');
assert.equal(app.view.selected, ids[0], 'not under the takeover');
assert.ok(takeover.contains(doc.activeElement), 'the arrows walk the takeover instead');
actionIn(takeover, 'enroll.cancel').focus();
key('Enter');
assert.equal(app.view.takeover, null);
app.emit('confirm', { kind: 'reset', identity: ids[0] });
assert.equal(doc.activeElement, actionIn(modal, 'modal.cancel'), 'a destructive sheet opens on Cancel');
assert.ok(legend.innerHTML.includes('Select') && !legend.innerHTML.includes('Confirm'), 'its legend reads Enter: Select');
key('ArrowDown'); key('ArrowDown');
assert.equal(app.view.selected, ids[0], 'not under a modal');
assert.ok(modal.contains(doc.activeElement));
// Tab cycles the modal's controls only.
for (let i = 0; i < 5; i++) { const ev = key('Tab', { shift: i % 2 === 1 }); assert.equal(ev.prevented, 1); assert.ok(modal.contains(doc.activeElement), 'Tab stays in the modal'); }
key('Escape');
advance(1000);
levelChange();
key('ArrowDown'); key('ArrowRight'); key('Tab');
assert.equal(app.view.selected, ids[0], 'not under a popup');
assert.equal(doc.activeElement, popup.querySelector('.btn'));
key('Escape');
advance(1000);
assert.equal(app.view.popup, null);

/* ---------- one key press, one action: keypress and keyup after a handled keydown go nowhere ---------- */
// Enter on Remove bond opens its sheet (focused on Cancel); the Char and KeyUp of that same press must not
// click the Cancel they now find focused.
const playerRow = RM.state.visibleRows(app.state).find(r => r.removable);
click(rowEl(RM.state.rowIdentity(playerRow)));
for (const k of ['Enter', ' ']) {
    actionIn(pane, 'remove').focus();
    sent.length = 0;
    const p = press(k);
    assert.equal(p.down.prevented, 1);
    assert.ok(p.keypress.prevented && p.keypress.stopped, JSON.stringify(k) + ': the keypress is cancelled');
    assert.ok(p.keyup.prevented && p.keyup.stopped, JSON.stringify(k) + ': the keyup is cancelled');
    assert.equal(app.view.modal && app.view.modal.kind, 'remove', JSON.stringify(k) + ' leaves the sheet open');
    assert.equal(doc.activeElement, actionIn(modal, 'modal.cancel'));
    assert.deepEqual(sent, [], 'unconfirmed, nothing sent');
    // With focus nowhere, Enter does nothing on a destructive sheet.
    doc.activeElement = null;
    press('Enter');
    assert.equal(app.view.modal && app.view.modal.kind, 'remove', 'Enter with focus nowhere does not confirm');
    assert.deepEqual(sent, []);
    key('Escape');
    advance(1000);
}
// A settings switch toggles once per press, Enter or Space; the switch keeps focus across the state that answers.
app.view.pane = 'settings';
app.render();
const gainSwitch = () => pane.querySelectorAll('[data-key]').find(n => n.getAttribute('data-key') === 'showGainModals');
const switchEl = gainSwitch();
for (const k of ['Enter', ' ']) {
    switchEl.focus();
    sent.length = 0;
    press(k);
    assert.deepEqual(of('romantasySettings'), [{ showGainModals: !app.state.showGainModals }], JSON.stringify(k) + ' toggles once');
    publish({ showGainModals: !app.state.showGainModals });
    assert.equal(gainSwitch(), switchEl, 'the state moves the switch in place');
    assert.equal(doc.activeElement, switchEl, 'and it keeps focus, so the next Enter works');
}
publish();
key('Escape');
// A stamp sets once per press.
app.emit('open-enroll', { mode: 'create' });
const LOC = 'ROM_LocationsDiscovered';
const stampOf = d => takeover.querySelectorAll('[data-of]').find(n => n.getAttribute('data-of') === LOC && n.getAttribute('data-direction') === String(d));
for (const k of ['Enter', ' ']) {
    stampOf(1).focus();
    press(k);
    assert.equal(app.view.takeover.staged[LOC], 1, JSON.stringify(k) + ' sets the like stamp once (twice would clear it)');
    click(stampOf(0));
}
// An unhandled key press is left alone: Enter typed in the search box reaches the field.
takeover.querySelector('.query').focus();
const typed = press('Enter');
assert.equal(typed.down.prevented + typed.keypress.prevented + typed.keyup.prevented, 0, 'a key the page did not handle is not cancelled');
key('Escape'); key('Escape');
assert.equal(app.view.takeover, null);

/* ---------- the back chain, one Escape at a time, sends what each click sends ---------- */
function stack() {
    app.view.pane = 'settings';
    app.render();
    app.emit('open-enroll', { mode: 'create' });
    app.emit('confirm', { kind: 'reset', identity: ids[0] });
    levelChange();
    frames();
}
const BUTTONS = [[popup, 'popup.dismiss'], [modal, 'modal.cancel'], [takeover, 'enroll.cancel'], [pane, 'settings.back'], [roster, 'close']];
function walkChain(step) {
    stack();
    const log = [];
    for (let i = 0; i < BUTTONS.length; i++) {
        const before = RM.pad.layer(app.view);
        sent.length = 0; sounds.length = 0;
        step(i);
        advance(1000);
        log.push({ before, after: RM.pad.layer(app.view), sent: plain(sent.filter(s => s[0] !== 'romantasyPopupShown')), sounds: sounds.slice() });
    }
    return log;
}
const byEscape = walkChain(() => { const ev = key('Escape'); assert.equal(ev.prevented, 1); });
assert.deepEqual(byEscape.map(s => s.before), ['popup', 'modal', 'takeover', 'settings', 'ledger']);
assert.deepEqual(byEscape.map(s => s.after), ['modal', 'takeover', 'settings', 'ledger', 'ledger']);
assert.deepEqual(byEscape[0].sent, [['romantasyPopupDismissed', { seq }]]);
assert.deepEqual(byEscape.slice(1, 4).map(s => s.sent), [[], [], []], 'modal, takeover and settings close in the page');
assert.deepEqual(byEscape[4].sent.map(x => x[0]), ['romantasyClose']);
const byClick = walkChain(i => click(actionIn(BUTTONS[i][0], BUTTONS[i][1])));
assert.deepEqual(byEscape.map(s => [s.before, s.after, s.sent.map(x => x[0]), s.sounds]),
    byClick.map(s => [s.before, s.after, s.sent.map(x => x[0]), s.sounds]), 'each Escape does what the matching click does');
assert.deepEqual(byEscape[4].sent, byClick[4].sent);

/* ---------- the close chord ---------- */
sent.length = 0;
let ev = chord();
assert.equal(ev.prevented, 1);
assert.equal(of('romantasyClose').length, 1, 'Ctrl + R closes the ledger');
chord({ repeat: true });
assert.equal(of('romantasyClose').length, 1, 'a held chord closes once');
// Inside the search box the chord is the field's: the ledger stays open, the stamps stay.
app.emit('open-enroll', { mode: 'create' });
click(takeover.querySelectorAll('[data-of]').find(n => n.getAttribute('data-of') === LOC && n.getAttribute('data-direction') === '1'));
const query = takeover.querySelector('.query');
query.focus();
sent.length = 0;
ev = chord();
assert.deepEqual(sent, [], 'no close while typing');
assert.equal(ev.prevented, 1, 'nor a reload of the page');
for (const k of ['a', ' ', 'ArrowDown', 'ArrowLeft', 'Home', 'End', 'Backspace', 'Enter']) {
    assert.equal(key(k).prevented, 0, k + ' belongs to the field');
}
assert.equal(doc.activeElement, query);
assert.equal(app.view.takeover.staged[LOC], 1);
// Escape leaves the field first, then cancels on the second press.
key('Escape');
assert.notEqual(doc.activeElement, query, 'the first Escape leaves the field');
assert.ok(app.view.takeover, 'and nothing else');
key('Escape');
assert.equal(app.view.takeover, null, 'the second goes back');
// Favorites mode: the hotkey is off, so the page does not close on it either.
publish({ openWithFavorites: true });
ev = chord();
assert.deepEqual(sent, [], 'no close in Favorites mode');
publish({ openWithFavorites: false });
// Other modifiers held: not the chord.
chord({ shift: true });
assert.deepEqual(sent, []);
// Popup mode: the ledger is not open, and the game's hotkey is muted while the page holds the keys. The chord
// closes nothing (no popup or queue dropped); it asks the plugin to open the ledger around the popup.
mode('popup');
levelChange();
frames();
assert.deepEqual(openScopes().map(s => s.options.root === byId('app') ? 'base' : 'layer'), ['layer'], 'no gamepad scope over the hidden ledger');
chord();
chord({ repeat: true });
assert.deepEqual(of('romantasyClose'), [], 'no close in popup mode');
assert.equal(of('romantasyOpen').length, 1, 'one romantasyOpen, none for the repeat');
publish({ openWithFavorites: true });
chord();
assert.equal(of('romantasyOpen').length, 1, 'none in Favorites mode');
publish({ openWithFavorites: false });
assert.ok(app.view.popup, 'the popup stays');
const before = app.view.popup, beforeMarkup = popup.innerHTML;
mode('ledger');   // the plugin answers with the ledger around the popup
assert.equal(app.view.popup, before, 'the same popup carries on over the ledger');
assert.equal(popup.innerHTML, beforeMarkup, 'not repainted, so its fuse is not restarted');
mode('popup');

/* ---------- a popup is dismissed once by Enter, Space or Escape, even when the key repeats ---------- */
for (const k of ['Enter', ' ', 'Escape']) {
    if (!app.view.popup) { levelChange(); frames(); }
    const mine = app.view.popup.seq;
    sent.length = 0;
    assert.equal(key(k).prevented, 1);
    key(k, { repeat: true });
    key(k);   // a second press inside the closing motion
    advance(1000);
    assert.deepEqual(of('romantasyPopupDismissed'), [{ seq: mine }], JSON.stringify(k) + ' dismisses once');
    assert.equal(app.view.popup, null);
}
// Between two popups (popup mode, none on screen) Escape sends nothing: no close of the queue.
sent.length = 0;
key('Escape');
assert.deepEqual(sent, []);
assert.equal(legend.innerHTML, '', 'no legend between popups');
levelChange();
frames();
assert.ok(legend.innerHTML.includes('Continue'), 'the popup legend shows in popup mode');
key('Enter');
advance(1000);
mode('ledger');

/* ---------- settings: keys and the pad at either end of the list scroll it to that end ---------- */
// Health (first while warn or error, else at the foot) holds no focus stop, so the ends bring it into view.
// The end-scroll comes after the control is brought into view (which on its own would leave the list short of
// its end), and never on a pointer's focus.
app.view.pane = 'settings';
app.render();
{
    const htmlEl = page.document.documentElement;
    const list = pane.querySelector('.setlist');
    const stops = list.querySelectorAll('button');
    assert.ok(stops.length >= 6, 'the switch rows are focus stops');
    // The list is 400 tall and holds 1200; the rows are 92 tall every 150, from 20.
    list.scrollHeight = 1200;
    layout = el => el === list ? { top: 0, height: 400 } : stops.includes(el) ? { top: 150 * stops.indexOf(el) + 20 - list.scrollTop, height: 92 } : null;
    // Keyboard: arrows and Tab go through the pad's focus(), which scrolls only the list, just enough.
    stops[1].focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, stops[2]);
    assert.equal(list.scrollTop, 150 * 2 + 20 + 92 - 400, 'a middle row is brought just into view');
    assert.deepEqual(pageScroll(), [0, 0, 0, 0], 'and nothing above the list scrolls');
    stops[1].focus();
    key('ArrowUp');
    assert.equal(doc.activeElement, stops[0]);
    assert.equal(list.scrollTop, 0, 'Up onto the first stop ends at the very start');
    stops[stops.length - 2].focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, stops[stops.length - 1]);
    assert.equal(list.scrollTop, 1200, 'Down onto the last stop ends at the very end');
    stops[stops.length - 1].focus();
    key('Tab', { shift: true });
    key('Tab');
    assert.equal(doc.activeElement, stops[stops.length - 1]);
    assert.equal(list.scrollTop, 1200, 'Tab onto the last stop ends at the very end');
    // Pointer: a click's focus on either end leaves the scroll where the player put it.
    list.scrollTop = 300;
    stops[0].focus();
    assert.equal(list.scrollTop, 300, 'a pointer focus on the first stop does not scroll');
    stops[stops.length - 1].focus();
    await null;
    assert.equal(list.scrollTop, 300, 'a pointer focus on the last stop does not scroll');
    // The pad: Magelight's helper focuses (preventScroll), then scrolls the control into view itself.
    // Here its scrollIntoView pulls the list to 77, short of either end.
    for (const stop of stops) stop.scrollIntoView = () => { stop.scrolled++; list.scrollTop = 77; };
    const padFocus = el => { el.focus(); el.scrollIntoView(); };
    htmlEl.setAttribute('data-ml-input', 'gamepad');
    padFocus(stops[0]);
    await null;
    assert.equal(list.scrollTop, 0, 'the pad onto the first stop ends at the very start');
    padFocus(stops[stops.length - 1]);
    await null;
    assert.equal(list.scrollTop, 1200, 'the pad onto the last stop ends at the very end');
    list.scrollTop = 300;
    padFocus(stops[2]);
    await null;
    assert.equal(list.scrollTop, 77, 'the pad on a middle row keeps what scrollIntoView did');
    htmlEl.setAttribute('data-ml-input', 'keyboardMouse');
    // Health first (a warning): the section is not a stop; Up onto the first switch still ends at the start.
    publish({ health: { status: 'warn', version: '3.0.1', host: '1.6.2', bonds: 2, items: [{ id: 'p', level: 'warn', title: 'T', detail: 'D' }] } });
    app.view.pane = 'dossier'; app.render(); app.view.pane = 'settings'; app.render();
    const warned = pane.querySelector('.setlist');
    assert.equal(warned.children[0].className, 'healthcard glass', 'Health comes first');
    assert.equal(warned.querySelectorAll('.healthcard')[0].querySelectorAll('button,[tabindex]').length, 0, 'Health holds no focus stop');
    const wstops = warned.querySelectorAll('button');
    // The first switch sits at 300 under Health; the list is scrolled past it, to 400.
    layout = el => el === warned ? { top: 0, height: 400 } : el === wstops[0] ? { top: 300 - warned.scrollTop, height: 92 } : null;
    wstops[1].focus();
    warned.scrollTop = 400;
    key('ArrowUp');
    assert.equal(doc.activeElement, wstops[0]);
    assert.equal(warned.scrollTop, 0, 'Health above the first switch shows from its top');
    // The roster is not touched: a row's focus leaves its list's scroll alone.
    const rowsList = roster.querySelector('.rows');
    rowsList.scrollTop = 40;
    app.view.pane = 'dossier'; app.render();
    rowEl(app.view.selected).focus();
    assert.equal(rowsList.scrollTop, 40);
    publish();
    app.view.pane = 'settings'; app.render();
    layout = () => null;
}

/* ---------- settings: a choice row is one stop; left and right move inside it; Enter, Space, A or a click choose ---------- */
{
    publish();
    app.view.pane = 'dossier'; app.render(); app.view.pane = 'settings'; app.render();
    const list = pane.querySelector('.setlist');
    const byKey = k => list.querySelectorAll('[role="switch"]').find(n => n.getAttribute('data-key') === k);
    const radio = (k, v) => list.querySelectorAll('[role="radio"]').find(n => n.getAttribute('data-key') === k && n.getAttribute('data-value') === v);
    const isChecked = el => el.getAttribute('aria-checked') === 'true';
    assert.ok(isChecked(radio('tierMoment', 'calm')) && isChecked(radio('pace', 'standard')), 'the defaults are checked');
    // Keys: Down from Show away companions lands on the first row's checked option; Left and Right stay in the row.
    sent.length = 0;
    byKey('showAwayFollowers').focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, radio('tierMoment', 'calm'), 'Down lands on the checked option of the first choice row');
    key('ArrowRight');
    assert.equal(doc.activeElement, radio('tierMoment', 'immediate'), 'Right moves to the next option');
    key('ArrowRight');
    assert.equal(doc.activeElement, radio('tierMoment', 'immediate'), 'Right at the row end stays in the row');
    key('ArrowLeft'); key('ArrowLeft');
    assert.equal(doc.activeElement, radio('tierMoment', 'calm'), 'Left at the row start stays in the row');
    assert.deepEqual(of('romantasySettings'), [], 'moving between options chooses nothing');
    key('ArrowRight');
    key('ArrowDown');
    assert.equal(doc.activeElement, radio('pace', 'standard'), 'Down from an option leaves the row for the next row\'s checked option');
    key('ArrowUp');
    assert.equal(doc.activeElement, radio('tierMoment', 'calm'), 'and Up comes back to the checked option, not the one last visited');
    key('ArrowUp');
    assert.equal(doc.activeElement, byKey('showAwayFollowers'));
    key('ArrowDown'); key('ArrowDown'); key('ArrowDown');
    assert.equal(doc.activeElement, byKey('whispers'), 'each choice row is one stop on the way down');
    key('ArrowDown'); key('ArrowDown');
    assert.equal(doc.activeElement, radio('ledgerSize', 'standard'), 'and Whisper corner is one stop before Ledger size');
    key('ArrowDown');
    assert.equal(doc.activeElement, byKey('highContrast'), 'Ledger size is one stop before Stronger contrast');
    key('ArrowDown');
    assert.equal(doc.activeElement, byKey('reduceMotion'));
    key('ArrowDown');
    assert.equal(doc.activeElement, byKey('detailedLog'), 'and Reduce motion one before Detailed log');
    // Tab: one stop per row too.
    byKey('showAwayFollowers').focus();
    key('Tab');
    assert.equal(doc.activeElement, radio('tierMoment', 'calm'));
    key('Tab');
    assert.equal(doc.activeElement, radio('pace', 'standard'));
    key('Tab');
    assert.equal(doc.activeElement, byKey('whispers'));
    key('Tab', { shift: true });
    assert.equal(doc.activeElement, radio('pace', 'standard'));
    // Enter or Space chooses the focused option, once; the state that answers checks it in place and it keeps the focus.
    for (const [k, value] of [['Enter', 'fast'], [' ', 'standard']]) {
        radio('pace', app.state.pace).focus();
        key(value === 'fast' ? 'ArrowRight' : 'ArrowLeft');
        const option = radio('pace', value);
        assert.equal(doc.activeElement, option);
        sent.length = 0;
        press(k);
        assert.deepEqual(of('romantasySettings'), [{ pace: value }], JSON.stringify(k) + ' chooses once');
        publish({ pace: value });
        assert.equal(radio('pace', value), option, 'the state checks the option in place');
        assert.ok(isChecked(option) && list.querySelectorAll('[role="radio"]').filter(n => n.getAttribute('data-key') === 'pace' && isChecked(n)).length === 1);
        assert.equal(doc.activeElement, option, 'and it keeps the focus');
    }
    sent.length = 0;
    press('Enter');
    assert.deepEqual(of('romantasySettings'), [], 'Enter on the checked option sends nothing');
    // The mouse: a click on any option chooses it.
    click(radio('tierMoment', 'immediate'));
    assert.deepEqual(of('romantasySettings'), [{ tierMoment: 'immediate' }]);
    publish({ tierMoment: 'immediate' });
    assert.ok(isChecked(radio('tierMoment', 'immediate')) && !isChecked(radio('tierMoment', 'calm')));
    // The pad: each row is one candidate (the checked option, or the option with the focus); Left and Right move
    // inside the row; A is the helper's click on the option; Up and Down in the pane walk the keys' list (the helper's
    // own move goes by position and would pass over a row whose checked option sits at its right end).
    const scope = topScope();
    byKey('showAwayFollowers').focus();
    const candidates = () => Array.from(scope.options.getCandidates()).filter(n => n.getAttribute('role') === 'radio' && pane.contains(n));   // the pane's rows (the roster's order choice is one too)
    assert.deepEqual(candidates(), [radio('tierMoment', 'immediate'), radio('pace', 'standard'), radio('whisperCorner', 'topRight'), radio('ledgerSize', 'standard')], 'the checked options only');
    const mark = el => { for (const n of list.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused'); el.setAttribute('data-ml-focused', ''); };
    radio('pace', 'standard').focus(); mark(radio('pace', 'standard'));
    assert.equal(scope.options.onAction({ action: 'right', phase: 'press' }), true, 'Right on an option is the page\'s');
    assert.equal(doc.activeElement, radio('pace', 'fast'));
    assert.deepEqual(candidates(), [radio('tierMoment', 'immediate'), radio('pace', 'fast'), radio('whisperCorner', 'topRight'), radio('ledgerSize', 'standard')], 'the focused option stands for its row');
    mark(radio('pace', 'fast'));
    assert.equal(scope.options.onAction({ action: 'right', phase: 'repeat' }), true, 'Right at the row end is held');
    assert.equal(doc.activeElement, radio('pace', 'fast'));
    assert.equal(scope.options.onAction({ action: 'left', phase: 'press' }), true);
    assert.equal(doc.activeElement, radio('pace', 'standard'));
    mark(radio('pace', 'standard'));
    assert.equal(scope.options.onAction({ action: 'left', phase: 'release' }), false, 'a release is not a move');
    assert.equal(scope.options.onAction({ action: 'accept', phase: 'press' }), false, 'A is the helper\'s click');
    assert.equal(scope.options.onAction({ action: 'down', phase: 'press' }), true, 'Down in the pane is the page\'s');
    assert.equal(doc.activeElement, byKey('whispers'), 'Down from an option goes to the next row');
    byKey('showAwayFollowers').focus(); mark(byKey('showAwayFollowers'));
    assert.equal(scope.options.onAction({ action: 'down', phase: 'repeat' }), true);
    assert.equal(doc.activeElement, radio('tierMoment', 'immediate'), 'Down from Show away companions lands on the checked option');
    mark(radio('tierMoment', 'immediate'));
    assert.equal(scope.options.onAction({ action: 'down', phase: 'press' }), true);
    assert.equal(doc.activeElement, radio('pace', 'standard'), 'and the next choice row is one stop too');
    mark(radio('pace', 'standard'));
    assert.equal(scope.options.onAction({ action: 'up', phase: 'press' }), true);
    assert.equal(doc.activeElement, radio('tierMoment', 'immediate'));
    assert.equal(scope.options.onAction({ action: 'down', phase: 'release' }), false, 'a release is not a move');
    for (const n of list.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused');
    rowEl(rows()[0]).setAttribute('data-ml-focused', '');
    assert.equal(scope.options.onAction({ action: 'down', phase: 'press' }), false, 'on the roster, Up and Down are the helper\'s');
    rowEl(rows()[0]).removeAttribute('data-ml-focused');
    byKey('detailedLog').focus(); mark(byKey('detailedLog'));
    assert.equal(scope.options.onAction({ action: 'right', phase: 'press' }), false, 'Right on a switch is the helper\'s');
    assert.equal(doc.activeElement, byKey('detailedLog'));
    for (const n of list.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused');
    publish();
}

/* ---------- settings: Whisper corner is four options while Whispers is on, and passed over while it is off ---------- */
// Off, the row stays drawn but dimmed: the group is aria-disabled and its options are disabled buttons, so the keys,
// Tab and the pad pass over it (Down from Whispers lands on Detailed log and Up comes back) and a click does nothing.
{
    publish();
    app.view.pane = 'dossier'; app.render(); app.view.pane = 'settings'; app.render();
    const list = pane.querySelector('.setlist');
    const byKey = k => list.querySelectorAll('[role="switch"]').find(n => n.getAttribute('data-key') === k);
    const radio = v => list.querySelectorAll('[role="radio"]').find(n => n.getAttribute('data-key') === 'whisperCorner' && n.getAttribute('data-value') === v);
    const group = () => radio('topRight').closest('[role="radiogroup"]');
    // The row after the corner: Ledger size, at its checked option.
    const size = () => list.querySelectorAll('[role="radio"]').find(n => n.getAttribute('data-key') === 'ledgerSize' && n.getAttribute('aria-checked') === 'true');
    const scope = topScope();
    const mark = el => { for (const n of list.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused'); el.setAttribute('data-ml-focused', ''); };
    assert.equal(app.state.whispers, true, 'whispers are on by default');
    assert.equal(radio('topRight').getAttribute('aria-checked'), 'true', 'Top right by default');
    assert.equal(group().getAttribute('aria-disabled'), null);
    // On: the keys reach all four options.
    byKey('whispers').focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, radio('topRight'), 'Down from Whispers lands on the checked corner');
    key('ArrowLeft');
    assert.equal(doc.activeElement, radio('topLeft'));
    key('ArrowRight'); key('ArrowRight');
    assert.equal(doc.activeElement, radio('bottomLeft'));
    key('ArrowRight');
    assert.equal(doc.activeElement, radio('bottomRight'), 'the keys reach the fourth option');
    sent.length = 0;
    press('Enter');
    assert.deepEqual(of('romantasySettings'), [{ whisperCorner: 'bottomRight' }], 'Enter chooses the corner');
    publish({ whisperCorner: 'bottomRight' });
    assert.equal(radio('bottomRight').getAttribute('aria-checked'), 'true');
    assert.equal(doc.activeElement, radio('bottomRight'), 'and it keeps the focus');
    key('ArrowDown');
    assert.equal(doc.activeElement, size());
    key('ArrowUp');
    assert.equal(doc.activeElement, radio('bottomRight'), 'Up comes back to the checked corner');
    // On: the pad reaches all four options too.
    radio('bottomRight').focus(); mark(radio('bottomRight'));
    for (const v of ['bottomLeft', 'topRight', 'topLeft']) {
        assert.equal(scope.options.onAction({ action: 'left', phase: 'press' }), true);
        assert.equal(doc.activeElement, radio(v), 'the pad moves left to ' + v);
        mark(radio(v));
    }
    for (const v of ['topRight', 'bottomLeft', 'bottomRight']) {
        assert.equal(scope.options.onAction({ action: 'right', phase: 'press' }), true);
        assert.equal(doc.activeElement, radio(v), 'the pad moves right to ' + v);
        mark(radio(v));
    }
    sent.length = 0;
    click(radio('topLeft'));   // A: the helper's click
    assert.deepEqual(of('romantasySettings'), [{ whisperCorner: 'topLeft' }]);
    publish({ whisperCorner: 'topLeft' });
    // Off: the same row, in place, dimmed and passed over.
    byKey('whispers').focus(); mark(byKey('whispers'));
    sent.length = 0;
    press('Enter');
    assert.deepEqual(of('romantasySettings'), [{ whispers: false }], 'Enter on Whispers turns them off');
    const topLeft = radio('topLeft');
    publish({ whispers: false, whisperCorner: 'topLeft' });
    assert.equal(radio('topLeft'), topLeft, 'the row is changed in place, not repainted');
    assert.equal(group().getAttribute('aria-disabled'), 'true', 'the group says it is disabled');
    assert.ok(['topLeft', 'topRight', 'bottomLeft', 'bottomRight'].every(v => radio(v).disabled), 'every option is disabled');
    assert.equal(radio('topLeft').getAttribute('aria-checked'), 'true', 'the chosen corner still shows');
    assert.equal(doc.activeElement, byKey('whispers'), 'Whispers keeps the focus');
    key('ArrowDown');
    assert.equal(doc.activeElement, size(), 'Down from Whispers passes over the corner row while off');
    key('ArrowUp');
    assert.equal(doc.activeElement, byKey('whispers'), 'and Up passes back over it');
    key('Tab');
    assert.equal(doc.activeElement, size(), 'Tab passes over it');
    key('Tab', { shift: true });
    assert.equal(doc.activeElement, byKey('whispers'));
    mark(byKey('whispers'));
    assert.equal(scope.options.onAction({ action: 'down', phase: 'press' }), true);
    assert.equal(doc.activeElement, size(), 'the pad passes over it going down');
    mark(size());
    assert.equal(scope.options.onAction({ action: 'up', phase: 'press' }), true);
    assert.equal(doc.activeElement, byKey('whispers'), 'and going up');
    assert.ok(!Array.from(scope.options.getCandidates()).some(n => n.getAttribute('data-key') === 'whisperCorner'), 'no corner is a pad candidate');
    sent.length = 0;
    click(radio('bottomLeft'));
    radio('bottomLeft').focus();
    press('Enter'); press(' ');
    assert.deepEqual(of('romantasySettings'), [], 'a disabled corner cannot be chosen by click, Enter or Space');
    // On again: the row comes back as it was.
    publish({ whispers: true, whisperCorner: 'topLeft' });
    assert.equal(group().getAttribute('aria-disabled'), null);
    assert.ok(['topLeft', 'topRight', 'bottomLeft', 'bottomRight'].every(v => !radio(v).disabled));
    byKey('whispers').focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, radio('topLeft'), 'Down from Whispers lands on the corner row again');
    for (const n of list.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused');
    publish();
    app.view.pane = 'dossier'; app.render();
}

/* ---------- settings: the SkyrimNet rows, only when SkyrimNet was found; the conversation row passed over while the bridge is off ---------- */
{
    const net = (bridge, conversation) => ({ skyrimNet: { found: true, bridge, conversation } });
    // Not found: Down from Whisper corner lands on Detailed log; there is no SkyrimNet stop.
    publish();
    app.view.pane = 'dossier'; app.render(); app.view.pane = 'settings'; app.render();
    let list = pane.querySelector('.setlist');
    const corner = () => list.querySelectorAll('[role="radio"]').find(n => n.getAttribute('data-key') === 'whisperCorner' && n.getAttribute('aria-checked') === 'true');
    let byKey = k => list.querySelectorAll('[role="switch"]').find(n => n.getAttribute('data-key') === k);
    const size = () => list.querySelectorAll('[role="radio"]').find(n => n.getAttribute('data-key') === 'ledgerSize' && n.getAttribute('aria-checked') === 'true');
    assert.equal(byKey('skyrimNetBridge'), undefined);
    corner().focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, size(), 'without SkyrimNet, Ledger size follows Whisper corner');
    // Found: both rows, in order, between Whisper corner and Detailed log.
    publish(net(true, true));
    app.view.pane = 'dossier'; app.render(); app.view.pane = 'settings'; app.render();
    list = pane.querySelector('.setlist');
    byKey = k => list.querySelectorAll('[role="switch"]').find(n => n.getAttribute('data-key') === k);
    const scope = topScope();
    const mark = el => { for (const n of list.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused'); el.setAttribute('data-ml-focused', ''); };
    corner().focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, byKey('skyrimNetBridge'), 'Down from Whisper corner lands on the bridge row');
    key('ArrowDown');
    assert.equal(doc.activeElement, byKey('skyrimNetConversation'));
    key('ArrowDown');
    assert.equal(doc.activeElement, size(), 'and the comfort rows follow the SkyrimNet rows');
    sent.length = 0;
    byKey('skyrimNetConversation').focus();
    press('Enter');
    assert.deepEqual(of('romantasySettings'), [{ skyrimNetConversation: false }], 'Enter toggles the conversation row once');
    publish(net(true, false));
    assert.equal(byKey('skyrimNetConversation').getAttribute('aria-checked'), 'false');
    // Bridge off: the conversation row stays drawn, disabled, and the keys, Tab and the pad pass over it.
    byKey('skyrimNetBridge').focus(); mark(byKey('skyrimNetBridge'));
    sent.length = 0;
    press('Enter');
    assert.deepEqual(of('romantasySettings'), [{ skyrimNetBridge: false }], 'Enter turns the bridge off');
    const conversation = byKey('skyrimNetConversation');
    publish(net(false, false));
    assert.equal(byKey('skyrimNetConversation'), conversation, 'the row is changed in place, not repainted');
    assert.ok(conversation.disabled, 'the conversation row is disabled while the bridge is off');
    assert.equal(conversation.getAttribute('aria-disabled'), 'true');
    assert.equal(doc.activeElement, byKey('skyrimNetBridge'), 'the bridge row keeps the focus');
    key('ArrowDown');
    assert.equal(doc.activeElement, size(), 'Down passes over the conversation row while the bridge is off');
    key('ArrowUp');
    assert.equal(doc.activeElement, byKey('skyrimNetBridge'), 'and Up passes back over it');
    key('Tab');
    assert.equal(doc.activeElement, size(), 'Tab passes over it');
    key('Tab', { shift: true });
    mark(byKey('skyrimNetBridge'));
    assert.equal(scope.options.onAction({ action: 'down', phase: 'press' }), true);
    assert.equal(doc.activeElement, size(), 'the pad passes over it');
    assert.ok(!Array.from(scope.options.getCandidates()).some(n => n.getAttribute('data-key') === 'skyrimNetConversation'), 'it is no pad candidate');
    sent.length = 0;
    click(conversation);
    assert.deepEqual(of('romantasySettings'), [], 'a disabled conversation row sends nothing on a click');
    // Bridge on again: the row comes back.
    publish(net(true, false));
    assert.ok(!byKey('skyrimNetConversation').disabled && byKey('skyrimNetConversation').getAttribute('aria-disabled') === null);
    byKey('skyrimNetBridge').focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, byKey('skyrimNetConversation'), 'Down from the bridge row lands on the conversation row again');
    for (const n of list.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused');
    publish();
    app.view.pane = 'dossier'; app.render();
}

/* ---------- settings: the OStim rows, only when OStim was found, after the SkyrimNet rows; never passed over ---------- */
{
    const both = (moments, hold) => ({ skyrimNet: { found: true, bridge: true, conversation: true }, ostim: { found: true, moments, hold } });
    publish({ skyrimNet: { found: true, bridge: true, conversation: true } });
    app.view.pane = 'dossier'; app.render(); app.view.pane = 'settings'; app.render();
    let list = pane.querySelector('.setlist');
    let byKey = k => list.querySelectorAll('[role="switch"]').find(n => n.getAttribute('data-key') === k);
    const size = () => list.querySelectorAll('[role="radio"]').find(n => n.getAttribute('data-key') === 'ledgerSize' && n.getAttribute('aria-checked') === 'true');
    // Not found: Down from the conversation row lands on Ledger size; there is no OStim stop.
    assert.equal(byKey('ostimMoments'), undefined);
    assert.equal(byKey('ostimHold'), undefined);
    byKey('skyrimNetConversation').focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, size(), 'without OStim, Ledger size follows the SkyrimNet rows');
    // Found: both rows, in order, between the SkyrimNet rows and Ledger size.
    publish(both(true, true));
    app.view.pane = 'dossier'; app.render(); app.view.pane = 'settings'; app.render();
    list = pane.querySelector('.setlist');
    byKey = k => list.querySelectorAll('[role="switch"]').find(n => n.getAttribute('data-key') === k);
    const scope = topScope();
    const mark = el => { for (const n of list.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused'); el.setAttribute('data-ml-focused', ''); };
    byKey('skyrimNetConversation').focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, byKey('ostimMoments'), 'Down from the conversation row lands on Shared moments');
    key('ArrowDown');
    assert.equal(doc.activeElement, byKey('ostimHold'));
    key('ArrowDown');
    assert.equal(doc.activeElement, size(), 'and Ledger size follows the OStim rows');
    key('ArrowUp');
    assert.equal(doc.activeElement, byKey('ostimHold'));
    key('Tab', { shift: true });
    assert.equal(doc.activeElement, byKey('ostimMoments'));
    // Enter toggles each once; with Shared moments off, Quiet during scenes is still a stop.
    sent.length = 0;
    press('Enter');
    assert.deepEqual(of('romantasySettings'), [{ ostimMoments: false }], 'Enter toggles Shared moments once');
    const hold = byKey('ostimHold');
    publish(both(false, true));
    assert.equal(byKey('ostimHold'), hold, 'the row is changed in place, not repainted');
    assert.equal(byKey('ostimMoments').getAttribute('aria-checked'), 'false');
    assert.ok(!hold.disabled && hold.getAttribute('aria-disabled') === null, 'the two rows are independent');
    assert.equal(doc.activeElement, byKey('ostimMoments'), 'Shared moments keeps the focus');
    mark(byKey('ostimMoments'));
    assert.equal(scope.options.onAction({ action: 'down', phase: 'press' }), true);
    assert.equal(doc.activeElement, hold, 'the pad lands on Quiet during scenes');
    sent.length = 0;
    press('Enter');
    assert.deepEqual(of('romantasySettings'), [{ ostimHold: false }], 'Enter toggles Quiet during scenes once');
    publish(both(true, false));
    assert.ok(!byKey('ostimMoments').disabled, 'Shared moments is never disabled');
    for (const n of list.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused');
    publish();
    app.view.pane = 'dossier'; app.render();
}

/* ---------- settings: the pad walks every stop, developer card included, in the keys' order ---------- */
// Down from the first stop to the last and Up back, through the page's pane walk (as the helper calls it, its mark
// following the focus as its focusin does): each stop once, none skipped, no trap; the list ends at its bottom on the
// last stop and at 0 on the first; #app and #pane never scroll.
{
    publish({ developerToolsUnlocked: true });
    app.view.pane = 'dossier'; app.render(); app.view.pane = 'settings'; app.render();
    const list = pane.querySelector('.setlist');
    assert.ok(list.querySelector('.devcard'), 'the developer card is open');
    const name = el => el.getAttribute('data-key') ? el.getAttribute('data-key') + (el.getAttribute('data-value') ? '=' + el.getAttribute('data-value') : '')
        : el.getAttribute('data-action');
    // The keys' order: Down from Back until it stops moving.
    const back = actionIn(pane, 'settings.back');
    const keyOrder = [back];
    back.focus();
    for (let i = 0; i < 40; i++) { key('ArrowDown'); if (doc.activeElement === keyOrder[keyOrder.length - 1]) break; keyOrder.push(doc.activeElement); }
    const names = keyOrder.map(name);
    assert.equal(new Set(names).size, names.length, 'the keys visit each stop once: ' + names.join(', '));
    for (const sw of RM.settings.SWITCHES) assert.ok(names.includes(sw[0]), 'the keys reach ' + sw[0]);
    for (const c of RM.settings.CHOICES) assert.equal(names.filter(n => n.startsWith(c[0] + '=')).length, 1, c[0] + ' is one stop');
    for (const d of RM.settings.DEV) assert.ok(names.includes(d[0]), 'the keys reach ' + d[0]);
    const after = n => names[names.indexOf(n) + 1];
    assert.ok(after('pace=standard') === 'whispers' && after('whispers') === 'whisperCorner=topRight' && after('whisperCorner=topRight') === 'ledgerSize=standard',
        'Whispers and Whisper corner sit between Relationship pace and Ledger size: ' + names.join(', '));
    assert.ok(after('ledgerSize=standard') === 'highContrast' && after('highContrast') === 'reduceMotion' && after('reduceMotion') === 'detailedLog',
        'the comfort rows sit together before Detailed log: ' + names.join(', '));
    // Layout: the list is 400 tall; its stops (all but Back) sit every 100 from 20, 60 tall, Health below them.
    const inList = keyOrder.slice(1);
    list.scrollHeight = 100 * inList.length + 300;
    list.scrollTop = 0;
    layout = el => el === list ? { top: 0, height: 400 } : inList.includes(el) ? { top: 100 * inList.indexOf(el) + 20 - list.scrollTop, height: 60 } : null;
    const scope = topScope();
    const mark = el => { for (const host of [pane, roster]) for (const n of host.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused'); el.setAttribute('data-ml-focused', ''); };
    const padStep = dir => { assert.equal(scope.options.onAction({ action: dir, phase: 'press' }), true, dir + ' in the pane is the page\'s'); mark(doc.activeElement); };
    back.focus(); mark(back);
    const down = [back];
    for (let i = 0; i < keyOrder.length + 3; i++) {
        padStep('down');
        if (doc.activeElement !== down[down.length - 1]) down.push(doc.activeElement);
        assert.deepEqual(pageScroll(), [0, 0, 0, 0], 'nothing above the list scrolls');
    }
    assert.deepEqual(down.map(name), names, 'the pad goes down in the keys\' order, every stop once');
    assert.equal(doc.activeElement, keyOrder[keyOrder.length - 1], 'and stays on the last at the end');
    assert.ok(list.scrollTop >= list.scrollHeight - 400, 'the list is at its bottom on the last stop');
    const up = [doc.activeElement];
    for (let i = 0; i < keyOrder.length + 3; i++) {
        padStep('up');
        if (doc.activeElement !== up[up.length - 1]) up.push(doc.activeElement);
        if (doc.activeElement === inList[0]) assert.equal(list.scrollTop, 0, 'the list is at 0 on its first stop');
        assert.deepEqual(pageScroll(), [0, 0, 0, 0], 'nothing above the list scrolls');
    }
    assert.deepEqual(up.map(name), names.slice().reverse(), 'and back up the same way: no trap');
    assert.equal(doc.activeElement, back);
    assert.equal(list.scrollTop, 0);
    // A choice the plugin refuses (the state answers with the old value): the focus stays on the option pressed.
    const radio = (k, v) => list.querySelectorAll('[role="radio"]').find(n => n.getAttribute('data-key') === k && n.getAttribute('data-value') === v);
    radio('pace', 'standard').focus(); mark(radio('pace', 'standard'));
    assert.equal(scope.options.onAction({ action: 'right', phase: 'press' }), true);
    const fast = radio('pace', 'fast');
    assert.equal(doc.activeElement, fast);
    mark(fast);
    sent.length = 0;
    click(fast);   // A: the helper's click
    assert.deepEqual(of('romantasySettings'), [{ pace: 'fast' }]);
    publish({ developerToolsUnlocked: true, pace: 'standard' });
    assert.equal(radio('pace', 'fast'), fast, 'the refusal repaints nothing');
    assert.equal(fast.getAttribute('aria-checked'), 'false', 'the old value stays checked');
    assert.equal(radio('pace', 'standard').getAttribute('aria-checked'), 'true');
    assert.equal(doc.activeElement, fast, 'the focus stays on the option that was pressed');
    for (const host of [pane, roster]) for (const n of host.querySelectorAll('[data-ml-focused]')) n.removeAttribute('data-ml-focused');
    layout = () => null;
    list.scrollTop = 0;
    publish();
    app.view.pane = 'dossier'; app.render();
}

/* ---------- a card body scrolls inside itself, by keys and by the pad; the page never scrolls ---------- */
{
    const heavy = plain(FIXTURE.romances);
    heavy[0].likes = Array.from({ length: 30 }, (_, i) => 'Deed ' + (i + 1));
    publish({ romances: heavy });
    app.view.pane = 'dossier';
    app.render();
    click(rowEl(RM.state.rowIdentity(heavy[0])));
    const [likes, dislikes] = pane.querySelectorAll('.list');
    assert.equal(likes.querySelectorAll('li').length, 30);
    assert.equal(likes.getAttribute('tabindex'), '0', 'the values list is a focus stop');
    likes.scrollHeight = 30 * 34;
    likes.clientHeight = 200;
    const end = 30 * 34 - 200;
    // Keys: Up and Down on the focused list scroll it by 60 stage pixels and keep the focus on it.
    likes.focus();
    key('ArrowDown');
    assert.equal(likes.scrollTop, 60, 'Down scrolls the list by 60');
    assert.equal(doc.activeElement, likes, 'and the list keeps the focus');
    key('ArrowDown'); key('ArrowUp');
    assert.equal(likes.scrollTop, 60);
    key('ArrowUp');
    assert.equal(likes.scrollTop, 0);
    for (let i = 0; i < Math.ceil(end / 60); i++) key('ArrowDown', { repeat: true });
    assert.equal(likes.scrollTop, end, 'it stops at the end of the list');
    assert.equal(doc.activeElement, likes);
    assert.deepEqual(pageScroll(), [0, 0, 0, 0], '#app, #pane, #roster and the document stay at 0');
    dislikes.scrollHeight = 600;   // a focus stop only while it has something to scroll
    dislikes.clientHeight = 200;
    key('ArrowDown');
    assert.equal(doc.activeElement, dislikes, 'at the end, Down moves on to the next stop');
    assert.equal(likes.scrollTop, end);
    // The bodies come after the pane's buttons: Right from the roster still lands on the first button, Journal (the
    // bond tools now sit in the Milestones head, below the cards), and Down walks on to the tools in their order.
    const player = RM.state.visibleRows(app.state).find(r => r.removable);
    click(rowEl(RM.state.rowIdentity(player)));
    rowEl(app.view.selected).focus();
    key('ArrowRight');
    assert.equal(doc.activeElement, actionIn(pane, 'journal.open'), 'the first pane stop is the Journal button');
    for (const action of ['edit', 'reset', 'remove']) {
        key('ArrowDown');
        assert.equal(doc.activeElement, actionIn(pane, action), 'Down reaches ' + action + ' in the Milestones head');
    }
    click(rowEl(rows()[0]));
    // The pad: the helper marks the control it is on; Up and Down on a marked body scroll it until its end.
    const again = pane.querySelectorAll('.list')[0];
    again.scrollHeight = 30 * 34;
    again.clientHeight = 200;
    again.setAttribute('data-ml-focused', '');
    const base = topScope();
    assert.equal(base.options.onAction({ action: 'down', phase: 'press' }), true, 'the pad scrolls the body it is on');
    assert.equal(again.scrollTop, 60);
    assert.equal(base.options.onAction({ action: 'down', phase: 'repeat' }), true);
    assert.equal(again.scrollTop, 120);
    assert.equal(base.options.onAction({ action: 'up', phase: 'press' }), true);
    assert.equal(again.scrollTop, 60);
    assert.equal(base.options.onAction({ action: 'down', phase: 'release' }), false, 'a release does nothing');
    again.scrollTop = end;
    assert.equal(base.options.onAction({ action: 'down', phase: 'press' }), false, 'at the end the helper moves on as usual');
    assert.equal(again.scrollTop, end);
    again.removeAttribute('data-ml-focused');
    rowEl(app.view.selected).setAttribute('data-ml-focused', '');
    assert.equal(base.options.onAction({ action: 'down', phase: 'press' }), false, 'on a row the helper moves as usual');
    rowEl(app.view.selected).removeAttribute('data-ml-focused');
    assert.deepEqual(pageScroll(), [0, 0, 0, 0]);
    // The popup's Unlocked list: Up and Down scroll it while Continue keeps the focus; Enter still continues.
    topics.get('romantasyLevelChange')({ followerName: 'Aelira', changeDirection: 'gain', levelName: 'Spouse', points: 2600 + seq, seq: ++seq,
        unlocks: ['One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven'] });
    frames();
    const unlocked = popup.querySelector('.scroller');
    assert.equal(unlocked.getAttribute('tabindex'), '0');
    unlocked.scrollHeight = 300;
    unlocked.clientHeight = 120;
    assert.equal(doc.activeElement, popup.querySelector('.btn'), 'Continue has the focus');
    key('ArrowDown');
    assert.equal(unlocked.scrollTop, 60, 'Down scrolls the Unlocked list');
    assert.equal(doc.activeElement, popup.querySelector('.btn'), 'and Continue keeps the focus');
    key('ArrowUp');
    assert.equal(unlocked.scrollTop, 0);
    assert.equal(topScope().options.onAction({ action: 'down', phase: 'press' }), true, 'the pad scrolls it too');
    assert.equal(unlocked.scrollTop, 60);
    assert.equal(doc.activeElement, popup.querySelector('.btn'));
    sent.length = 0;
    press('Enter');
    advance(1000);
    assert.equal(app.view.popup, null, 'Enter continues');
    assert.equal(of('romantasyPopupDismissed').length, 1);
    // The guard: anything that scrolls the page or the stage is put back at once; a card's own body keeps its scroll.
    assert.equal(typeof docListeners.scroll, 'function', 'the scroll guard listens on document');
    assert.equal(docCapture.scroll, true, 'in the capture phase, since scroll events do not bubble');
    const ownBody = pane.querySelectorAll('.list')[0], ownRows = roster.querySelector('.rows');
    ownBody.scrollTop = 60;
    ownRows.scrollTop = 30;
    byId('app').scrollTop = 50;
    pane.scrollTop = 12;
    roster.scrollTop = 9;
    page.document.documentElement.scrollTop = 4;
    docListeners.scroll({ target: byId('app') });
    assert.deepEqual(pageScroll(), [0, 0, 0, 0], 'the scroll guard puts #app, #pane, #roster and the document back to 0');
    assert.equal(ownBody.scrollTop, 60, 'a card body keeps its scroll');
    assert.equal(ownRows.scrollTop, 30, 'and so does the roster list');
    docListeners.scroll({ target: ownBody });
    assert.equal(ownBody.scrollTop, 60, 'its own scroll event moves nothing back');
    publish();
}

/* ---------- a scrolling body with nothing to scroll is no focus stop, for keys or the pad ---------- */
{
    const heavy = plain(FIXTURE.romances);
    heavy[0].likes = Array.from({ length: 30 }, (_, i) => 'Deed ' + (i + 1));
    publish({ romances: heavy });
    app.view.pane = 'dossier';
    app.render();
    click(rowEl(RM.state.rowIdentity(heavy[0])));
    const body = id => pane.querySelectorAll('[data-ml-id]').find(n => n.getAttribute('data-ml-id') === id);
    // [scrollHeight, clientHeight] by data-ml-id: the likes and the Personality text overflow; the dislikes fit
    // exactly and Recent changes to within a pixel (a rounding, not a scroll).
    const sizes = { 'body-likes': [1020, 200], 'body-dislikes': [200, 200], 'body-recent': [201, 200], 'body-persona': [500, 200] };
    layout = el => { const s = sizes[el.getAttribute('data-ml-id')]; return s ? { top: 0, height: s[1], scrollHeight: s[0] } : null; };
    const candidates = () => [...topScope().options.getCandidates()].map(el => el.getAttribute('data-ml-id')).filter(id => /^body-/.test(id || ''));
    assert.deepEqual(candidates(), ['body-likes', 'body-persona'], 'the pad is offered only the bodies that scroll');
    // Keys: Down at the end of the likes passes over the two bodies that fit; Up comes back; Tab does the same.
    body('body-likes').focus();
    body('body-likes').scrollTop = 1020 - 200;
    key('ArrowDown');
    assert.equal(doc.activeElement, body('body-persona'), 'Down skips the bodies with nothing to scroll');
    key('ArrowUp');
    assert.equal(doc.activeElement, body('body-likes'), 'and Up skips them back');
    key('Tab');
    assert.equal(doc.activeElement, body('body-persona'), 'Tab skips them too');
    // A click can still focus a body that fits: the keys step on from where it is, to its neighbours either side.
    for (const [k, o, to] of [['ArrowDown', {}, 'body-persona'], ['ArrowUp', {}, 'body-likes'], ['ArrowRight', {}, 'body-persona'],
        ['ArrowLeft', {}, 'body-likes'], ['Tab', {}, 'body-persona'], ['Tab', { shift: true }, 'body-likes']]) {
        body('body-dislikes').focus();
        key(k, o);
        assert.equal(doc.activeElement && (doc.activeElement.getAttribute('data-ml-id') || doc.activeElement.getAttribute('data-action')), to, `${k}${o.shift ? ' with Shift' : ''} from a fitting body lands on its neighbour`);
    }
    // The same body with a long list is a stop again.
    sizes['body-dislikes'] = [600, 200];
    assert.deepEqual(candidates(), ['body-likes', 'body-dislikes', 'body-persona'], 'a long list is offered to the pad');
    body('body-likes').focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, body('body-dislikes'), 'and reached by Down');
    // A repaint that leaves the focused body with nothing to scroll: the focus goes on to the next body that scrolls.
    sizes['body-recent'] = [600, 200];
    body('body-recent').focus();
    sizes['body-recent'] = [150, 200];
    const shorter = plain(heavy);
    shorter[0].recent = shorter[0].recent.slice(0, 1);
    publish({ romances: shorter });
    assert.equal(doc.activeElement, body('body-persona'), 'focus lands on the next body that scrolls');
    // None after it: the one before it. None at all: the selected row.
    sizes['body-persona'] = [100, 200];
    publish({ romances: heavy });
    assert.equal(doc.activeElement, body('body-dislikes'), 'or on the one before it');
    sizes['body-dislikes'] = [100, 200];
    sizes['body-likes'] = [100, 200];
    publish({ romances: shorter });
    assert.equal(doc.activeElement, rowEl(app.view.selected), 'or, when no body scrolls, on the selected row');
    assert.ok(pane.querySelectorAll('[data-ml-id]').filter(n => /^body-/.test(n.getAttribute('data-ml-id'))).every(n => n.getAttribute('tabindex') === '0'),
        'the bodies keep their tabindex');
    layout = () => null;
    publish();
}

/* ---------- the password field: Enter submits once; Escape blurs, then cancels ---------- */
app.view.pane = 'settings';
app.render();
const devSwitch = pane.querySelectorAll('[data-key]').find(n => n.getAttribute('data-key') === 'developerToolsUnlocked');
devSwitch.focus();
press('Enter');
assert.equal(app.view.modal.kind, 'password');
const field = modal.querySelector('.field');
assert.equal(doc.activeElement, field);
field.value = 'mock';
sent.length = 0;
assert.equal(key('Enter').prevented, 1);
key('Enter', { repeat: true });
key('Enter');   // while the answer is pending
assert.deepEqual(of('romantasyDevUnlock'), [{ password: 'mock' }], 'Enter submits once');
publish({ developerToolsUnlocked: false });   // refused: the field is cleared and focused again
assert.equal(doc.activeElement, field);
key('Escape');
assert.ok(app.view.modal, 'the first Escape only leaves the field');
key('Escape');
assert.equal(app.view.modal, null, 'the second cancels the sheet');
assert.equal(app.view.pane, 'settings', 'and only the sheet');
assert.equal(of('romantasyClose').length, 0);
assert.equal(doc.activeElement, devSwitch, 'focus returns to the developer-tools switch');
advance(1000);
// Focus on nothing (a click on the sheet's text): Enter unlocks the password sheet, Escape cancels any sheet.
press('Enter');
modal.querySelector('.field').value = 'x';
doc.activeElement = null;
sent.length = 0;
press('Enter');
assert.deepEqual(of('romantasyDevUnlock'), [{ password: 'x' }], 'Enter unlocks with focus nowhere');
publish({ developerToolsUnlocked: false });
key('Escape'); key('Escape');
advance(1000);
app.view.pane = 'dossier';
app.render();
app.emit('confirm', { kind: 'remove', identity: ids[0] });
doc.activeElement = null;
key('Escape');
assert.equal(app.view.modal, null, 'Escape cancels with focus nowhere');
advance(1000);
// Enter on a focused Cancel cancels: the focused control wins.
app.emit('confirm', { kind: 'remove', identity: ids[0] });
actionIn(modal, 'modal.cancel').focus();
sent.length = 0;
key('Enter');
assert.equal(app.view.modal, null);
assert.deepEqual(sent, []);
advance(1000);

/* ---------- a popup over the enrollment search box gives the focus back to the box ---------- */
app.emit('open-enroll', { mode: 'create' });
const searchBox = takeover.querySelector('.query');
searchBox.focus();
levelChange();
frames();
assert.equal(doc.activeElement, popup.querySelector('.btn'));
press('Enter');
advance(1000);
assert.equal(app.view.popup, null);
assert.equal(doc.activeElement, searchBox, 'focus returns to the search box');
key('Escape'); key('Escape');
assert.equal(app.view.takeover, null);

/* ---------- composition and repeats never double-act ---------- */
app.emit('confirm', { kind: 'reset', identity: ids[0] });
sent.length = 0;
assert.equal(key('Enter', { composing: true }).prevented, 0);
key('Enter', { repeat: true });
key('Escape', { composing: true });
assert.deepEqual(sent, [], 'an IME composition or a repeat does nothing');
assert.ok(app.view.modal);
key('Escape', { repeat: true });
assert.ok(app.view.modal, 'a repeated Escape is not a new press');
key('Escape');
advance(1000);

/* ---------- gamepad: one scope per layer; B is Escape, layer by layer ---------- */
const open = () => openScopes().map(s => s.options.root === byId('app') ? 'base' : [...elements].find(([, el]) => el === s.options.root)[0]);
assert.deepEqual(open(), ['base']);
// The pane switching to settings attaches the base scope afresh, so the pad starts on the first switch.
app.view.pane = 'dossier';
app.render();
const ledgerScope = topScope();
assert.equal(ledgerScope.options.initialFocus(), rowEl(app.view.selected));
app.view.pane = 'settings';
app.render();
assert.equal(ledgerScope.open, false, 'the ledger scope goes');
assert.notEqual(topScope(), ledgerScope);
assert.equal(topScope().options.initialFocus(), pane.querySelector('[role="switch"]'));
app.view.pane = 'dossier';
app.render();
assert.equal(topScope().options.initialFocus(), rowEl(app.view.selected));
stack();
assert.deepEqual(open(), ['base', 'takeover', 'modal', 'popup'], 'a scope for each open layer, the top one last');
assert.equal(topScope().options.initialFocus(), popup.querySelector('.btn'), 'the popup starts on Continue');
const byB = [];
for (let i = 0; i < BUTTONS.length; i++) {
    const before = RM.pad.layer(app.view);
    if (before === 'modal') assert.equal(topScope().options.initialFocus(), actionIn(modal, 'modal.cancel'), 'a destructive sheet starts on Cancel');
    if (before === 'settings' || before === 'ledger') {
        assert.equal(topScope().options.initialFocus(), before === 'settings' ? pane.querySelector('[role="switch"]') : rowEl(app.view.selected));
    }
    sent.length = 0; sounds.length = 0;
    assert.equal(topScope().options.onBack(), true);
    advance(1000);
    byB.push({ before, after: RM.pad.layer(app.view), sent: plain(sent.filter(s => s[0] !== 'romantasyPopupShown')).map(x => x[0]), sounds: sounds.slice() });
}
assert.deepEqual(byB, byEscape.map(s => ({ before: s.before, after: s.after, sent: s.sent.map(x => x[0]), sounds: s.sounds })), 'B does what Escape does in every layer');
assert.deepEqual(open(), ['base']);
// A on a deed row stamps it, as Enter does.
app.emit('open-enroll', { mode: 'create' });
const deed = takeover.querySelectorAll('[data-deed]').find(n => n.getAttribute('data-deed') === LOC);
deed.setAttribute('data-ml-focused', '');
assert.equal(topScope().options.onAction({ action: 'accept', phase: 'press' }), true);
assert.equal(app.view.takeover.staged[LOC], 1);
assert.equal(topScope().options.onAction({ action: 'accept', phase: 'release' }), false);
// The takeover's pad stops are its key stops: a summary column that fits is passed over, one that scrolls is offered.
{
    const [fits, scrolls] = takeover.querySelectorAll('.chips');
    fits.scrollHeight = 80; fits.clientHeight = 120;
    scrolls.scrollHeight = 400; scrolls.clientHeight = 120;
    const offered = [...topScope().options.getCandidates()];
    assert.ok(!offered.includes(fits), 'a summary column that fits is no pad stop');
    assert.ok(offered.includes(scrolls), 'one that scrolls is');
    assert.ok(offered.includes(takeover.querySelector('[data-deed]')) && offered.includes(takeover.querySelector('[data-candidate]')), 'the deeds and candidates are still offered');
    fits.scrollHeight = 400;
    assert.ok([...topScope().options.getCandidates()].includes(fits), 'and the first too, once it has a long list');
}
key('Escape');
// Hidden: every scope goes, and keys do nothing.
RM.bridge.focused = false;
topics.get('__uimode')(false);
advance(1000);
assert.deepEqual(open(), []);
sent.length = 0;
chord(); key('Enter'); key('ArrowDown');
assert.deepEqual(sent, [], 'a hidden page acts on no key');
assert.equal(legend.innerHTML, '');
// Escape is the one exception: keys reach a hidden page only when it is on screen after all (it missed a mode),
// so Escape asks the plugin to close (close-only there). Once per press.
key('Escape');
key('Escape', { repeat: true });
assert.deepEqual(sent, [['romantasyClose', undefined]], 'Escape on a page that believes it is hidden asks to close');

/* ---------- every open, in C++'s order (mode, then state), starts on the selected bond ---------- */
// The page is never reloaded: the last session's roster is still in the DOM while hidden.
const reopen = extra => { RM.bridge.focused = true; topics.get('__uimode')(true); mode('ledger'); publish(extra); };
const hide = () => { RM.bridge.focused = false; topics.get('__uimode')(false); advance(300); };
const closeByClick = () => { const b = actionIn(roster, 'close'); b.focus(); click(b); hide(); };
const selectedEl = () => rowEl(app.view.selected);
const ticked = n => ({ romances: plain(FIXTURE.romances).map(r => ({ ...r, points: r.points + n })) });
reopen();
closeByClick();
// A roster that changed while closed is repainted by the first state.
reopen(ticked(7));
assert.ok(selectedEl(), 'the roster is painted');
assert.equal(doc.activeElement, selectedEl(), 'a repainted roster: the selected row has focus');
sent.length = 0;
key('Enter');
assert.equal(of('romantasyClose').length, 0, 'and Enter does not close the ledger');
closeByClick();
// The same roster again: not repainted, and the focus is not left on Close.
reopen(ticked(7));
assert.equal(doc.activeElement, selectedEl(), 'an unchanged roster: the selected row, not Close');
// A refresh while open that repaints the roster keeps the focus on the same bond's row.
key('ArrowDown');
const bond = app.view.selected;
assert.equal(doc.activeElement, rowEl(bond));
const oldRow = rowEl(bond);
publish(ticked(19));
assert.notEqual(rowEl(bond), oldRow, 'the roster was repainted');
assert.equal(doc.activeElement, rowEl(bond), 'a repaint keeps the focus on the same bond');
// A refresh while typing in the enrollment search box leaves the focus and the caret in the box.
app.emit('open-enroll', { mode: 'create' });
const box = takeover.querySelector('.query');
box.focus();
box.value = 'ki';
box.selectionStart = box.selectionEnd = 1;
publish(ticked(23));
assert.equal(takeover.querySelector('.query'), box);
assert.equal(doc.activeElement, box, 'the box keeps the focus');
assert.equal(box.selectionStart, 1, 'and its caret');
// A refresh that repaints the candidate list keeps the focus on the same candidate.
const cand = takeover.querySelectorAll('[data-candidate]')[1];
cand.focus();
publish({ ...ticked(23), enrollmentCandidates: [...FIXTURE.enrollmentCandidates, { name: 'Lydia', role: 'Nord · Housecarl', sourcePlugin: 'Skyrim.esm', referenceFormID: '000A2C94', baseFormID: '000A2C8E' }] });
assert.equal(doc.activeElement.getAttribute('data-ml-id'), cand.getAttribute('data-ml-id'), 'the same candidate keeps the focus');

/* ---------- self-heal sends: never in normal operation ---------- */
// C++'s order everywhere: romantasyMode is published right after its request, UI mode comes on when the host runs
// the entry; on a close "hidden" goes out before the release, whose __uimode off follows.
const uimode = on => { RM.bridge.focused = on; topics.get('__uimode')(on); };
const ready = () => of('romantasyReady').length, closes = () => of('romantasyClose').length;
uimode(false);
advance(300);
sent.length = 0;
// The ledger opens and closes with Escape: the close is the player's, nothing else goes out.
mode('ledger'); uimode(true); publish();
key('Escape');
assert.equal(closes(), 1, 'the player\'s close');
mode('hidden');
uimode(false);
key('Escape');                                  // during the release: nothing
advance(300);
assert.equal(ready(), 0, 'a normal open and close re-sends no ready');
assert.equal(closes(), 1, 'and Escape during the release sends nothing');
sent.length = 0;
// A popup alone, dismissed; then a popup the player turns into the ledger with Ctrl + R.
mode('popup'); uimode(true); levelChange(); frames();
press('Enter');
advance(300);
assert.equal(of('romantasyPopupDismissed').length, 1);
mode('hidden'); uimode(false); advance(300);
mode('popup'); uimode(true); levelChange(); frames();
chord();
assert.equal(of('romantasyOpen').length, 1);
mode('ledger');                                 // retargeted in place: no __uimode change
topics.get('romantasyLevelChange')({ followerName: 'Aelira', changeDirection: 'gain', levelName: 'Lover', points: 2010 + seq - 1, seq });
publish();
frames(); advance(1000);
assert.equal(ready(), 0, 'popups and the retarget re-send no ready');
assert.equal(closes(), 0, 'and send no close');
// A blink (__uimode off and on inside 250 ms) is not a missed mode either.
uimode(false); advance(100); key('Escape'); uimode(true);
assert.equal(ready(), 0, 'a blink re-sends no ready');
assert.equal(closes(), 0, 'and Escape inside the blink sends no close');
// The page missed its mode: told hidden, yet UI mode comes on with no release to undo. It asks again.
mode('hidden');
uimode(true);
assert.equal(ready(), 1, 'UI mode on while hidden re-sends romantasyReady');

/* ---------- the Journal pane: open, the answer, the wait, Back, another bond, keys and the pad ---------- */
mode('ledger');
publish();
advance(2000);
{
    const answer = (identity, extra) => ({ identity, found: true, began: 2, daysTogether: 3.5,
        milestones: [{ kind: 'began', level: 1, day: 2 }, { kind: 'reached', level: 2, day: 3 }], days: [{ day: 2, gained: 5, lost: 0 }],
        gains: [{ label: 'Deed one', points: 5, day: 2 }], losses: [], history: [{ label: 'Deed one', points: 5, day: 2 }], ...extra });
    const reply = data => topics.get('romantasyJournal')(data);
    const journalButton = () => actionIn(pane, 'journal.open');
    const backButton = () => actionIn(pane, 'journal.back');
    const OPENING = 'Opening the journal…', FAILED = 'The journal could not be opened.';
    const [first, second] = rows();
    click(rowEl(first));
    assert.equal(app.view.pane, 'dossier');
    assert.ok(journalButton(), 'the dossier has a Journal button');
    assert.equal(journalButton().getAttribute('data-target'), first, 'for the selected bond');
    // Keys reach it: Right from the roster into the dossier, then Down; and the pad is offered it.
    rowEl(first).focus();
    key('ArrowRight');
    for (let i = 0; i < 10 && doc.activeElement !== journalButton(); i++) key('ArrowDown');
    assert.equal(doc.activeElement, journalButton(), 'the keys reach the Journal button');
    assert.ok([...topScope().options.getCandidates()].includes(journalButton()), 'and the pad is offered it');

    // Enter opens the pane, asks for the selected bond's journal and waits; the pane starts on Back.
    sent.length = 0;
    press('Enter');
    assert.equal(app.view.pane, 'journal');
    assert.equal(RM.pad.layer(app.view), 'journal');
    assert.deepEqual(of('romantasyJournalOpen'), [{ identity: first }], 'one request, with the bond\'s identity');
    assert.ok(pane.innerHTML.includes(OPENING), 'the waiting text');
    assert.equal(pane.className.split(' ')[0], 'journal');
    assert.equal(doc.activeElement, backButton(), 'the pane opens on Back');
    assert.ok(legend.innerHTML.includes('Scroll') && legend.innerHTML.includes('Back') && !legend.innerHTML.includes('Close'), 'its legend: Scroll and Back');
    // An answer for another bond is not this one.
    reply(answer(second, { history: [{ label: 'Not hers', points: 1, day: 2 }] }));
    assert.ok(pane.innerHTML.includes(OPENING) && !pane.innerHTML.includes('Not hers'), 'an answer for another identity is ignored');
    reply(answer(first));
    assert.ok(!pane.innerHTML.includes(OPENING) && pane.innerHTML.includes('Day 3 · Reached Acquaintance') && pane.innerHTML.includes('Deed one'), 'the answer fills the pane');
    advance(5000);
    assert.ok(!pane.innerHTML.includes(FAILED), 'the wait ended with the answer');
    assert.equal(doc.activeElement, backButton(), 'Back keeps the focus across the answer');

    // A state refresh for the same bond while the pane is open: the pane and its data stay, the body keeps its scroll,
    // nothing is asked again, and the focus stays on Back. Both when the head is unchanged (points ticking) and when it
    // is repainted (the tier's name changed).
    {
        layout = el => el.getAttribute('data-ml-id') === 'body-journal' ? { top: 0, height: 300, scrollHeight: 1500 } : null;
        pane.querySelector('.jbody').scrollTop = 240;
        const before = pane.querySelector('.jbody');
        const refresh = extra => publish({ romances: plain(FIXTURE.romances).map(r => RM.state.rowIdentity(r) === first ? { ...r, ...extra } : r) });
        sent.length = 0;
        refresh({ points: FIXTURE.romances.find(r => RM.state.rowIdentity(r) === first).points + 3 });
        assert.equal(app.view.pane, 'journal', 'a refresh keeps the Journal open');
        assert.equal(pane.querySelector('.jbody'), before, 'an unchanged pane is not repainted');
        assert.equal(pane.querySelector('.jbody').scrollTop, 240, 'its body keeps its scroll');
        assert.ok(pane.innerHTML.includes('Deed one'), 'and its data');
        assert.equal(doc.activeElement, backButton(), 'and the focus stays on Back');
        refresh({ levelName: 'Sworn Friend' });
        assert.equal(app.view.pane, 'journal');
        assert.notEqual(pane.querySelector('.jbody'), before, 'a changed head repaints the pane');
        assert.ok(pane.innerHTML.includes('Sworn Friend') && pane.innerHTML.includes('Day 3 · Reached Acquaintance') && pane.innerHTML.includes('Deed one'),
            'with the new tier and the same journal');
        assert.ok(!pane.innerHTML.includes(OPENING), 'not waiting again');
        assert.equal(pane.querySelector('.jbody').scrollTop, 240, 'the repainted body keeps its scroll');
        assert.equal(doc.activeElement, backButton(), 'the repainted Back has the focus');
        assert.deepEqual(of('romantasyJournalOpen'), [], 'no second request');
        advance(5000);
        assert.ok(!pane.innerHTML.includes(FAILED), 'and no wait was started');
        publish();
        pane.querySelector('.jbody').scrollTop = 0;
        layout = () => null;
    }

    // The body scrolls with the keys and the pad, and nothing above it does.
    const body = () => pane.querySelector('.jbody');
    layout = el => el.getAttribute('data-ml-id') === 'body-journal' ? { top: 0, height: 300, scrollHeight: 1500 } : null;
    key('ArrowDown');
    assert.equal(doc.activeElement, body(), 'Down from Back reaches the body');
    key('ArrowDown');
    assert.equal(body().scrollTop, 60, 'and Down scrolls it');
    key('ArrowUp'); key('ArrowUp');
    assert.equal(doc.activeElement, backButton(), 'Up at its top goes back to Back');
    assert.ok([...topScope().options.getCandidates()].includes(body()), 'the pad is offered the body');
    assert.equal(topScope().options.initialFocus(), backButton(), 'the pad starts on Back');
    body().setAttribute('data-ml-focused', '');
    assert.equal(topScope().options.onAction({ action: 'down', phase: 'press' }), true);
    assert.equal(body().scrollTop, 60, 'the pad scrolls the body');
    body().removeAttribute('data-ml-focused');
    assert.deepEqual(pageScroll(), [0, 0, 0, 0], 'the page never scrolls');
    layout = () => null;

    // Escape (or B, or a click on Back) returns to the dossier, focus on its Journal button.
    sent.length = 0;
    key('Escape');
    assert.equal(app.view.pane, 'dossier');
    assert.deepEqual(sent, [], 'Back closes in the page');
    assert.equal(doc.activeElement, journalButton(), 'focus returns to the Journal button');
    click(journalButton());
    assert.equal(topScope().options.onBack(), true);
    assert.equal(app.view.pane, 'dossier', 'B goes back');
    click(journalButton());
    click(backButton());
    assert.equal(app.view.pane, 'dossier', 'and so does a click on Back');

    // No answer: after 3 seconds the pane says so; a late answer for this bond still fills it.
    click(journalButton());
    advance(2999);
    assert.ok(pane.innerHTML.includes(OPENING));
    advance(1);
    assert.ok(pane.innerHTML.includes(FAILED) && !pane.innerHTML.includes(OPENING), 'the failure text after 3 seconds');
    reply(answer(first));
    assert.ok(pane.innerHTML.includes('Deed one') && !pane.innerHTML.includes(FAILED), 'a late answer fills the pane');
    // found false is the failure at once.
    key('Escape');
    click(journalButton());
    reply({ identity: first, found: false });
    assert.ok(pane.innerHTML.includes(FAILED));
    // Closing the pane ends the wait: reopened a second later, the first wait's end does not fail the second.
    key('Escape');
    click(journalButton());
    advance(1000);
    key('Escape');
    click(journalButton());
    advance(2500);
    assert.ok(pane.innerHTML.includes(OPENING), 'the closed pane\'s wait is cancelled');
    advance(500);
    assert.ok(pane.innerHTML.includes(FAILED));

    // Another bond chosen in the roster while the journal is open: that bond's dossier; the old wait and answer are dropped.
    key('Escape');
    click(journalButton());
    click(rowEl(second));
    assert.equal(app.view.pane, 'dossier');
    assert.equal(app.view.selected, second);
    assert.equal(journalButton().getAttribute('data-target'), second, 'the new bond\'s dossier');
    reply(answer(first));
    advance(5000);
    assert.equal(app.view.pane, 'dossier', 'a late answer for the old bond opens nothing');
    // The same with the keys: Tab to a row, Enter; the focus lands on the chosen row.
    click(journalButton());
    for (let i = 0; i < 40 && doc.activeElement !== rowEl(first); i++) key('Tab');
    assert.equal(doc.activeElement, rowEl(first), 'Tab reaches the roster');
    key('Enter');
    assert.equal(app.view.pane, 'dossier');
    assert.equal(app.view.selected, first);
    assert.equal(doc.activeElement, rowEl(first), 'focus on the chosen row');
    // A state that takes the bond away (removed) ends its journal.
    click(journalButton());
    publish({ romances: plain(FIXTURE.romances).filter(r => RM.state.rowIdentity(r) !== first) });
    assert.equal(app.view.pane, 'dossier', 'the bond is gone: back to the dossier');
    advance(5000);
    publish();
}

/* ---------- the roster: find, order and groups ---------- */
{
    mode('hidden'); mode('ledger'); publish(); advance(2000);
    const field = () => roster.querySelector('.findbox');
    const clearButton = () => actionIn(roster, 'roster.clear');
    const option = v => roster.querySelectorAll('[role="radio"]').find(n => n.getAttribute('data-value') === v);
    const shown = () => roster.querySelectorAll('[data-identity]').map(el => el.getAttribute('data-identity'));
    const nameOf = id => FIXTURE.romances.find(r => RM.state.rowIdentity(r) === id).name;
    const type = text => { field().value = text; roster.listeners.input({ target: field() }); };
    const all = shown();
    assert.equal(all.length, FIXTURE.romances.length, 'every bond shows');
    assert.deepEqual([...rows()], all, 'the walk follows the rows as drawn');
    const box = field();
    assert.ok(box && box.tagName === 'INPUT');
    assert.equal(clearButton().disabled, true, 'nothing to clear yet');

    // Typing filters; the field is never repainted and keeps the focus; the page's own keys stay out of it.
    box.focus();
    sent.length = 0;
    type('aeli');
    assert.deepEqual(shown().map(nameOf), ['Aelira'], 'typing filters');
    assert.equal(field(), box, 'the field is the same element');
    assert.equal(doc.activeElement, box);
    assert.equal(clearButton().disabled, false);
    for (const k of ['Enter', ' ', 'ArrowDown', 'ArrowUp', 'ArrowLeft', 'ArrowRight']) {
        const ev = key(k);
        assert.equal(ev.prevented, 0, JSON.stringify(k) + ' is the field\'s');
    }
    chord();
    assert.equal(doc.activeElement, box, 'the keys leave the focus in the field');
    assert.deepEqual(sent, [], 'and send nothing; typing sends nothing');
    assert.equal(app.view.selected, all.find(id => nameOf(id) === 'Aelira'));

    // Escape in a field with text clears it and stays; in an empty one it leaves the field; the ledger stays open.
    key('Escape');
    assert.equal(box.value, '');
    assert.deepEqual(shown(), all, 'Escape cleared the find');
    assert.equal(doc.activeElement, box, 'and kept the field');
    key('Escape');
    assert.notEqual(doc.activeElement, box, 'Escape in the empty field leaves it');
    assert.equal(doc.activeElement, rowEl(app.view.selected), 'for the selected row, never body');
    // The control remembered for the ledger is that row now: a repaint that drops the focus returns it there.
    doc.activeElement = null;
    publish();
    assert.equal(doc.activeElement, rowEl(app.view.selected), 'a repaint returns the focus to the row, not the field');
    assert.equal(app.view.mode, 'ledger');
    assert.deepEqual(sent, [], 'nothing closed');

    // Regex characters and other alphabets match literally.
    type('СЕР');
    assert.deepEqual(shown().map(nameOf), ['Серана <b>']);
    type('(');
    assert.deepEqual(shown(), []);

    // No match: the roster says so, nothing is selected, the pane shows its empty state.
    type('zzz');
    assert.deepEqual(shown(), []);
    assert.ok(roster.querySelector('.rows').innerHTML.includes('No one by that name.'));
    assert.equal(app.view.selected, null);
    assert.ok(pane.innerHTML.includes('No one by that name.') && !pane.innerHTML.includes('journal.open'), 'the empty pane, without an error');
    // Clear restores every row and selects the first. A click leaves the focus in the find field, never on the
    // button it turned off (an older WebKit may not have focused the button at all).
    doc.activeElement = null;
    click(clearButton());
    assert.equal(box.value, '');
    assert.deepEqual(shown(), all, 'clear restores');
    assert.equal(app.view.selected, all[0]);
    assert.equal(doc.activeElement, box, 'after a click the focus is in the find field');
    assert.equal(clearButton().disabled, true);
    // The keys: Enter on the clear button does the same.
    type('ly');
    clearButton().focus();
    press('Enter');
    assert.deepEqual(shown(), all);
    assert.equal(doc.activeElement, box, 'after Enter the focus is in the find field');

    // The selection stays through a filter that keeps it.
    const lydia = all.find(id => nameOf(id) === 'Lydia');
    click(rowEl(lydia));
    type('y');
    assert.equal(app.view.selected, lydia, 'Lydia still shows, still selected');
    type('');

    // Order: a choice sends the one key; the rows move when the state brings it back; the selection stays.
    sent.length = 0;
    click(option('closest'));
    assert.deepEqual(of('romantasySettings'), [], 'the current order sends nothing');
    click(option('name'));
    assert.deepEqual(of('romantasySettings'), [{ rosterOrder: 'name' }]);
    assert.deepEqual(shown(), all, 'nothing moves before the state answers');
    publish({ rosterOrder: 'name' });
    assert.equal(option('name').getAttribute('aria-checked'), 'true');
    const byName = group => FIXTURE.romances.filter(r => r.isFollowing === group).map(r => r.name).sort((a, b) => a.localeCompare(b));
    assert.deepEqual(shown().map(nameOf), [...byName(true), ...byName(false)], 'by name inside each group');
    assert.equal(app.view.selected, lydia, 'the selection stays through a re-order');

    // A state update moves no row (review focus 1), even when points would sort it elsewhere.
    publish({ rosterOrder: 'closest' });
    const closest = shown();
    const last = closest[closest.length - 1];
    publish({ romances: plain(FIXTURE.romances).map(r => RM.state.rowIdentity(r) === last ? { ...r, points: 99999 } : r) });
    assert.deepEqual(shown(), closest, 'points changed; places did not');
    // A bond that changes group goes last in its new group, and no other row moves; so does hiding or showing the away rows.
    {
        const base = plain(FIXTURE.romances).map(r => RM.state.rowIdentity(r) === last ? { ...r, points: 99999 } : r);
        const rowOf = id => base.find(r => RM.state.rowIdentity(r) === id);
        const f0 = closest.filter(id => rowOf(id).isFollowing), a0 = closest.filter(id => !rowOf(id).isFollowing);
        const mover = f0[0];
        const flipped = base.map(r => RM.state.rowIdentity(r) === mover ? { ...r, isFollowing: false } : r);
        publish({ romances: flipped });
        assert.deepEqual(shown(), [...f0.slice(1), ...a0, mover], 'Following to Away: last in Away; nothing else moves');
        publish({ romances: flipped, showAwayFollowers: false });
        assert.deepEqual(shown(), f0.slice(1), 'away rows hidden: the following rows stay put');
        publish({ romances: flipped, showAwayFollowers: true });
        const points = id => (RM.state.rowIdentity(rowOf(id)) === mover ? rowOf(mover) : rowOf(id)).points;
        const back = [...a0, mover].sort((x, y) => (points(y) - points(x)) || rowOf(x).name.localeCompare(rowOf(y).name));
        assert.deepEqual(shown(), [...f0.slice(1), ...back], 'shown again: the following rows stay put, the away rows return after them in Closest order');
        publish({ romances: base });
        assert.deepEqual(shown(), [...f0.slice(1), mover, ...back.filter(id => id !== mover)], 'Away to Following: last in Following; nothing else moves');
    }
    // Opening the ledger again sorts afresh and clears the find text.
    type('ae');
    mode('hidden'); mode('ledger');
    assert.equal(app.view.find, '');
    assert.equal(field().value, '', 'opening the ledger clears the find text');
    assert.notDeepEqual(shown(), closest, 'and sorts afresh');
    publish();
    // The plugin's order on a reopen: the ledger closes, points change while it is shut, then the mode comes before
    // the state that carries them. The order is made from that state, not pinned to the last one of the old session.
    {
        const before = shown();
        const lastShown = before[before.length - 1];
        const raised = plain(FIXTURE.romances).map(r => RM.state.rowIdentity(r) === lastShown ? { ...r, points: 99999 } : r);
        const fresh = [...RM.roster.arrange(RM.state.normalize({ romances: raised }).romances, { order: 'closest', showAway: true }).order];
        assert.notDeepEqual(fresh, before, 'the raised bond sorts elsewhere');
        hide();
        mode('ledger');
        publish({ romances: raised });
        assert.deepEqual(shown(), fresh, 'the first state after a reopen is sorted afresh');
        // A later state while it is open moves no row.
        publish();
        assert.deepEqual(shown(), fresh, 'a later state while open keeps the places');
        RM.bridge.focused = true;
        topics.get('__uimode')(true);
    }
    publish();

    // The Journal: a filter that keeps its bond keeps it open; one that hides it goes back to the new selection's dossier,
    // and asks the plugin for nothing.
    const aelira = all.find(id => nameOf(id) === 'Aelira');
    click(rowEl(aelira));
    click(actionIn(pane, 'journal.open'));
    assert.equal(app.view.pane, 'journal');
    sent.length = 0;
    type('ae');
    assert.equal(app.view.pane, 'journal', 'still her journal');
    type('lyd');
    assert.equal(app.view.pane, 'dossier', 'her row is gone: the dossier');
    assert.equal(app.view.selected, lydia);
    assert.equal(actionIn(pane, 'journal.open').getAttribute('data-target'), lydia, 'of the newly selected row');
    assert.deepEqual(sent, [], 'nothing asked of the plugin');
    // In Settings a filter changes the selection only.
    app.emit('open-settings');
    type('ae');
    assert.equal(app.view.pane, 'settings');
    assert.equal(app.view.selected, aelira);
    type('');
    actionIn(pane, 'settings.back') && click(actionIn(pane, 'settings.back'));

    // The pad: offered the order choice (one stop), the clear button once there is text, and every row; never the field.
    app.view.pane = 'dossier'; app.render();
    const offered = () => [...topScope().options.getCandidates()];
    assert.ok(!offered().includes(field()), 'the pad never lands on the text field');
    assert.ok(offered().includes(option('closest')), 'the order choice is a pad stop');
    assert.ok(!offered().includes(option('name')), 'as one stop');
    for (const id of shown()) assert.ok(offered().includes(rowEl(id)), 'every row is a pad stop');
    type('a');
    assert.ok(offered().includes(clearButton()), 'the clear button is a pad stop');
    assert.ok(!offered().includes(field()));
    // A on the clear button clears (the helper's click) and moves the focus to the selected row, never the field.
    const htmlEl = page.document.documentElement;
    htmlEl.setAttribute('data-ml-input', 'gamepad');
    clearButton().focus();
    click(clearButton());
    assert.deepEqual(shown(), all);
    assert.equal(doc.activeElement, rowEl(app.view.selected), 'after a clear by the pad the focus is on the selected row');
    htmlEl.setAttribute('data-ml-input', 'keyboardMouse');
    // Left and Right on the order choice step through it, as the keys do.
    option('closest').setAttribute('data-ml-focused', '');
    assert.equal(topScope().options.onAction({ action: 'right', phase: 'press' }), true);
    assert.equal(doc.activeElement, option('name'));
    option('closest').removeAttribute('data-ml-focused');
    // The keyboard: Tab reaches the field (it can type), and Down from the head goes to the selected row.
    for (let i = 0; i < 40 && doc.activeElement !== field(); i++) key('Tab');
    assert.equal(doc.activeElement, field(), 'Tab reaches the field');
    option('closest').focus();
    key('ArrowDown');
    assert.equal(doc.activeElement, rowEl(app.view.selected), 'Down from the head goes to the selected row');
    publish();
}

/* ---------- a new Ledger size brings the control in use back into view, inside its own list ---------- */
{
    mode('ledger');
    publish();
    app.view.pane = 'dossier'; app.render(); app.view.pane = 'settings'; app.render();
    const htmlEl = page.document.documentElement;
    const list = pane.querySelector('.setlist');
    const option = v => list.querySelectorAll('[role="radio"]').find(n => n.getAttribute('data-key') === 'ledgerSize' && n.getAttribute('data-value') === v);
    // The list is 400 tall; at Largest every row is taller, so the size row sits further down it than at Standard.
    list.scrollHeight = 2000;
    layout = el => el === list ? { top: 0, height: 400 }
        : el === option('largest') ? { top: (htmlEl.getAttribute('data-size') === 'largest' ? 1500 : 700) - list.scrollTop, height: 50 } : null;
    option('largest').focus();
    list.scrollTop = 400;
    publish({ ledgerSize: 'largest' });
    assert.equal(doc.activeElement, option('largest'), 'the chosen option keeps the focus');
    assert.equal(list.scrollTop, 1500 + 50 - 400, 'a new size scrolls its list just enough to show it');
    assert.deepEqual(pageScroll(), [0, 0, 0, 0], 'and nothing above the list scrolls');
    list.scrollTop = 0;
    publish({ ledgerSize: 'largest' });
    assert.equal(list.scrollTop, 0, 'the same size again moves nothing');
    layout = () => null;
    publish();
    app.view.pane = 'dossier'; app.render();
}

/* ---------- Reduce motion: the points show at once, with no count-up ---------- */
{
    const count = () => pane.querySelector('[data-count]');
    const others = () => rows().filter(id => id !== app.view.selected);
    click(rowEl(others()[0]));
    assert.equal(count().textContent, '0', 'with motion the points count up from 0');
    publish({ reduceMotion: true });
    click(rowEl(others()[0]));
    const shownCount = count();
    assert.notEqual(shownCount.textContent, '0', 'under Reduce motion the count does not start at 0');
    advance(2000);
    assert.equal(shownCount.textContent, undefined, 'and nothing ticks: the number in the markup stays');
    publish();
}

console.log('InputFlow: ok');
