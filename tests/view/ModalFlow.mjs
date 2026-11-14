// The modal and notice DOM flows, under a stand-in DOM that knows the few elements
// these parts reach for, and a fake clock.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const fixtureBox = { window: {} };
vm.runInNewContext(readFileSync(new URL('../../view/dev/fixture.js', import.meta.url), 'utf8'), fixtureBox);
const FIXTURE = fixtureBox.window.ROMANTASY_FIXTURE;
const plain = v => v === undefined || v === null ? v : JSON.parse(JSON.stringify(v));

/* ---------- fake clock ---------- */
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

/* ---------- stand-in DOM ---------- */
function element() {
    const attrs = {}, classes = new Set(), listeners = {};
    const el = {
        innerHTML: '', textContent: '', value: '', style: {}, offsetWidth: 0, isConnected: true, disabled: false, added: [], focused: 0,
        get className() { return [...classes].join(' '); },
        set className(v) { classes.clear(); for (const c of String(v).split(/\s+/)) if (c) classes.add(c); },
        classList: {
            add: c => { classes.add(c); el.added.push(c); }, remove: c => classes.delete(c), contains: c => classes.has(c),
            toggle: (c, on) => (on ?? !classes.has(c)) ? classes.add(c) : classes.delete(c)
        },
        setAttribute: (k, v) => { attrs[k] = String(v); }, getAttribute: k => (k in attrs ? attrs[k] : null), removeAttribute: k => { delete attrs[k]; },
        querySelector: () => null, querySelectorAll: () => [], appendChild() {}, remove() {}, insertAdjacentHTML() {},
        addEventListener: (type, fn) => { listeners[type] = fn; }, listeners,
        focus() { el.focused++; }
    };
    return el;
}
// #modal: setting innerHTML builds the parts modals.js looks up by selector.
function modalHost() {
    const el = element();
    let html = '', parts = {};
    const errorLine = () => { const e = element(); e.remove = () => { delete parts['.error']; }; return e; };
    Object.defineProperty(el, 'innerHTML', {
        get: () => html,
        set: v => {
            html = v; parts = {};
            if (v.includes('class="sheet')) parts['.sheet'] = element();
            if (v.includes('class="field"')) {
                const field = element();
                field.className = 'field';
                field.insertAdjacentHTML = (where, h) => { if (h.includes('class="error"')) parts['.error'] = errorLine(); };
                parts['.field'] = field;
            }
            if (v.includes('class="error"')) parts['.error'] = errorLine();
            for (const a of ['modal.unlock', 'modal.cancel', 'modal.confirm']) {
                if (v.includes(`data-action="${a}"`)) parts[`[data-action="${a}"]`] = element();
            }
        }
    });
    el.querySelector = selector => parts[selector] || null;
    el.parts = () => parts;
    return el;
}

const elements = new Map([['modal', modalHost()]]);
const sent = [];
let sendResult = true, replyInSend = null;
const topics = new Map();
const page = {
    console, setTimeout: setTimeoutFake, clearTimeout: clearTimeoutFake,
    document: {
        readyState: 'complete', documentElement: element(),
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
        send: (name, payload) => { sent.push([name, plain(payload)]); if (replyInSend) replyInSend(); return sendResult; },
        sound() {}
    }
};
for (const file of ['strings.js', 'tiers.js', 'format.js', 'state.js', 'notice.js', 'roster.js', 'ledger.js', 'settings.js', 'modals.js', 'app.js']) {
    vm.runInNewContext(readFileSync(new URL('../../view/js/' + file, import.meta.url), 'utf8'), page);
}
const RM = page.RM, app = RM.app;
const modal = elements.get('modal');
let revision = 1;
const publish = extra => topics.get('romantasyState')({ ...plain(FIXTURE), revision: ++revision, message: '', ...extra });
const act = name => { for (const p of RM.parts) if (p.actions && p.actions[name]) return p.actions[name](element()); throw new Error('no action ' + name); };
const shakes = () => { const sheet = modal.querySelector('.sheet'); return sheet ? sheet.added.filter(c => c === 'shake').length : 0; };
const unlock = () => modal.querySelector('[data-action="modal.unlock"]');
publish();
advance(5000);

/* ---------- the password reply shakes once, on a wrong answer only ---------- */
app.view.modal = { kind: 'password' };
app.render();
assert.ok(modal.classList.contains('open'));
assert.equal(modal.querySelector('.field').focused, 1, 'the field takes focus');
RM.modals.submit('wrong');
assert.equal(app.view.modal.pending, true);
assert.equal(unlock().disabled, true, 'Unlock waits while the host answers');
assert.equal(shakes(), 0, 'no shake before the answer');
publish({ developerToolsUnlocked: false });
assert.equal(app.view.modal.error, true);
assert.ok(modal.querySelector('.error'), 'the error line shows');
assert.equal(modal.querySelector('.field').value, '');
assert.equal(unlock().disabled, false);
assert.equal(shakes(), 1, 'a wrong answer shakes once');
publish({ developerToolsUnlocked: false });   // an unrelated state while the error shows
app.render();
assert.equal(shakes(), 1, 'an unrelated render does not shake again');
assert.ok(modal.querySelector('.error'), 'the error line stays until the next attempt');
RM.modals.submit('mock');
assert.ok(!app.view.modal.error, 'a new attempt clears the error');
assert.equal(modal.querySelector('.error'), null, 'and its line');
assert.equal(shakes(), 1, 'the second submit does not shake');
publish({ developerToolsUnlocked: false });
assert.equal(shakes(), 2, 'a second wrong answer shakes again, once');
RM.modals.submit('mock');
publish({ developerToolsUnlocked: true });
assert.equal(app.view.modal, null, 'the right answer closes the sheet');
assert.equal(shakes(), 2, 'without a shake');
advance(1000);

/* ---------- an unlocked state closes the sheet also when nothing is pending ---------- */
// A state that lands between Unlock and its answer reads as a refusal; the real answer after it still closes.
publish({ developerToolsUnlocked: false });
app.view.modal = { kind: 'password' };
app.render();
RM.modals.submit('mock');
publish({ developerToolsUnlocked: false });   // an unrelated state (a refresh) before the answer
assert.equal(app.view.modal.pending, false);
publish({ developerToolsUnlocked: true });    // the answer
assert.equal(app.view.modal, null, 'the unlocked state closes the sheet');
advance(1000);
// A sheet opened with nothing sent closes too once the tools are unlocked.
publish({ developerToolsUnlocked: false });
app.view.modal = { kind: 'password' };
app.render();
publish({ developerToolsUnlocked: true });
assert.equal(app.view.modal, null, 'not pending: closed all the same');
advance(1000);
publish({ developerToolsUnlocked: false });

/* ---------- mode "hidden" clears an open sheet at once ---------- */
app.emit('confirm', { kind: 'reset', identity: 'FE964801' });
app.emit('open-settings');
assert.ok(app.view.modal && app.view.pane === 'settings');
topics.get('romantasyMode')({ mode: 'hidden' });
assert.equal(app.view.mode, 'hidden');
assert.equal(app.view.modal, null, 'the modal is cleared');
assert.equal(app.view.pane, 'dossier', 'the pane is back on the dossier');
advance(220);
assert.ok(!modal.classList.contains('open'), 'and the sheet is taken off');
topics.get('romantasyMode')({ mode: 'ledger' });
advance(1000);

/* ---------- no host: Unlock is not left waiting ---------- */
publish({ developerToolsUnlocked: false });
app.view.modal = { kind: 'password' };
app.render();
sendResult = false;
RM.modals.submit('x');
assert.ok(!app.view.modal.pending, 'nothing pending when the send failed');
assert.equal(unlock().disabled, false);
sendResult = true;
act('modal.cancel');
advance(1000);

/* ---------- a host that answers inside send() (the browser harness) ---------- */
publish({ developerToolsUnlocked: false });
app.view.modal = { kind: 'password' };
app.render();
replyInSend = () => publish({ developerToolsUnlocked: false });
RM.modals.submit('wrong');
assert.equal(app.view.modal.error, true, 'an answer given during the send still reads as refused');
assert.ok(!app.view.modal.pending, 'and nothing is left waiting');
assert.equal(unlock().disabled, false);
replyInSend = () => publish({ developerToolsUnlocked: true });
RM.modals.submit('mock');
assert.equal(app.view.modal, null, 'an answer given during the send unlocks');
replyInSend = null;
publish({ developerToolsUnlocked: false });
advance(1000);

/* ---------- a confirm sheet closes when its bond is gone ---------- */
{
    const gone = 'FE964801';   // Inalion: removable, away
    const without = () => plain(FIXTURE).romances.filter(r => r.referenceFormID !== gone);
    for (const kind of ['reset', 'remove']) {
        app.emit('confirm', { kind, identity: gone });
        assert.equal(app.view.modal.kind, kind);
        publish({ showAwayFollowers: false });   // still in the ledger, only hidden by the filter
        assert.equal(app.view.modal && app.view.modal.kind, kind, kind + ': a bond hidden as away keeps its sheet');
        const before = sent.length;
        publish({ romances: without() });
        assert.equal(app.view.modal, null, kind + ': the sheet closes once its bond is gone');
        assert.equal(sent.length, before, kind + ': and sends nothing');
        advance(1000);
    }
    app.view.modal = { kind: 'password' };
    app.render();
    publish({ romances: without() });
    assert.equal(app.view.modal && app.view.modal.kind, 'password', 'the password sheet is unaffected');
    act('modal.cancel');
    publish();
    advance(1000);
}

/* ---------- close sequence ---------- */
const identity = 'FE964801';
app.emit('confirm', { kind: 'reset', identity });
assert.ok(modal.classList.contains('open'));
assert.ok(modal.innerHTML.includes('Reset this bond?'));
act('modal.cancel');
assert.ok(modal.classList.contains('closing') && modal.classList.contains('open'), 'closing, still drawn');
advance(219);
assert.ok(modal.classList.contains('open'));
advance(1);
assert.ok(!modal.classList.contains('open') && !modal.classList.contains('closing'));
assert.equal(modal.innerHTML, '');
// a new sheet opened inside the closing window survives the old timer
app.emit('confirm', { kind: 'reset', identity });
act('modal.cancel');
advance(100);
app.emit('confirm', { kind: 'remove', identity });
assert.ok(modal.classList.contains('open') && !modal.classList.contains('closing'));
advance(500);
assert.ok(modal.classList.contains('open'));
assert.ok(modal.innerHTML.includes('Remove this bond?'));
act('modal.cancel');
advance(1000);

/* ---------- keys ---------- */
// The modal keeps no key listener of its own: Enter in the field, Enter-as-confirm and Escape
// belong to pad.js's one listener on document (tests/view/InputFlow.mjs drives them).
assert.equal(modal.listeners.keydown, undefined, 'the modal keeps no key listener of its own');

/* ---------- notice ---------- */
const notice = page.document.getElementById('notice');
const HOLD = RM.notice.HOLD_MS;
publish({ message: 'Saved <b>' });
assert.ok(notice.classList.contains('show'));
assert.ok(notice.innerHTML.includes('Saved &lt;b&gt;') && !notice.innerHTML.includes('<b>'), 'the message is escaped');
assert.ok(!notice.classList.contains('warning'));
advance(HOLD - 1000);
app.render();   // the same revision again: not shown anew, the timer not restarted
assert.equal(notice.added.filter(c => c === 'show').length, 1);
advance(999);
assert.ok(notice.classList.contains('show'));
advance(1);
assert.ok(!notice.classList.contains('show'), 'hidden after HOLD_MS');
publish({ message: 'That bond cannot be reset here.' });
assert.ok(notice.classList.contains('warning') && notice.classList.contains('show'));
advance(HOLD);
const before = notice.innerHTML;
publish({ message: '' });
assert.ok(!notice.classList.contains('show'), 'an empty message shows nothing');
assert.equal(notice.innerHTML, before);
assert.equal(app.view.lastRevision, revision);

console.log('ModalFlow: ok');
