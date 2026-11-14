// The enrollment takeover's DOM flows: the real page scripts in a vm, under a small
// stand-in DOM that parses the markup the takeover writes into a tree, and a fake clock.
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

/* ---------- stand-in DOM: a tree built from the markup, simple selectors only ---------- */
const doc = { activeElement: null };
const VOID = new Set(['img', 'input', 'br', 'path']);
const decode = v => v.replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&');
function element(tag) {
    const attrs = {}, listeners = {};
    let html = '';
    const el = {
        tagName: (tag || 'div').toUpperCase(), parent: null, children: [], attrs, listeners, style: {},
        scrollTop: 0, offsetWidth: 0, isConnected: true, disabled: false, value: '',
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
        focus() { doc.activeElement = el; },
        remove() {}, appendChild() {}, insertAdjacentHTML() {}
    };
    return el;
}
function detach(n) { n.isConnected = false; if (doc.activeElement === n) doc.activeElement = null; for (const c of n.children) detach(c); }
function walk(root, fn) { for (const c of root.children) { fn(c); walk(c, fn); } }
// One compound selector: tag, .class and [attr] / [attr="value"] parts; commas allowed.
function matches(el, selector) {
    return selector.split(',').some(sel => {
        const m = sel.trim().match(/^([a-z]+)?((?:\.[\w-]+|\[[\w-]+(?:="[^"]*")?\])*)$/);
        if (!m) throw new Error('stand-in DOM cannot match ' + sel);
        if (m[1] && el.tagName.toLowerCase() !== m[1]) return false;
        for (const p of m[2].matchAll(/\.([\w-]+)|\[([\w-]+)(?:="([^"]*)")?\]/g)) {
            if (p[1] && !el.classList.contains(p[1])) return false;
            if (p[2] && (el.getAttribute(p[2]) === null || (p[3] !== undefined && el.getAttribute(p[2]) !== p[3]))) return false;
        }
        return true;
    });
}

const elements = new Map();
const sent = [];
let sendResult = true;
const topics = new Map();
const page = {
    console, setTimeout: setTimeoutFake, clearTimeout: clearTimeoutFake,
    document: {
        readyState: 'complete', documentElement: element('html'),
        get activeElement() { return doc.activeElement; },
        getElementById: id => { if (!elements.has(id)) elements.set(id, element()); return elements.get(id); },
        querySelector: () => null, createElement: element,
        listeners: {}, addEventListener(type, fn) { page.document.listeners[type] = fn; }
    }
};
page.window = page;
page.addEventListener = () => {};   // pad.js cancels a handled key's keypress and keyup on window
page.RM = {
    fit: { apply() {} },
    bridge: {
        focused: true,
        on: (topic, fn) => topics.set(topic, fn),
        send: (name, payload) => { sent.push([name, plain(payload)]); return sendResult; },
        sound() {}
    }
};
for (const file of ['strings.js', 'tiers.js', 'format.js', 'state.js', 'notice.js', 'enrollment.js', 'roster.js', 'ledger.js', 'settings.js', 'modals.js', 'pad.js', 'app.js']) {
    vm.runInNewContext(readFileSync(new URL('../../view/js/' + file, import.meta.url), 'utf8'), page);
}
const RM = page.RM, app = RM.app, e = RM.enrollment;
const takeover = page.document.getElementById('takeover');
let revision = 1;
const publish = extra => topics.get('romantasyState')({ ...plain(FIXTURE), revision: ++revision, message: '', ...extra });
// A click as app.js dispatches it: the nearest [data-action] runs the first part that names it.
function click(el) {
    const hit = el.closest('[data-action]');
    assert.ok(hit, 'clicked a control');
    if (hit.disabled) return;
    for (const p of RM.parts) if (p.actions && p.actions[hit.getAttribute('data-action')]) return p.actions[hit.getAttribute('data-action')](hit);
    throw new Error('no action ' + hit.getAttribute('data-action'));
}
const find = (attr, value, root = takeover) => root.querySelectorAll('[' + attr + ']').find(n => n.getAttribute(attr) === value) || null;
const stamp = (id, d) => takeover.querySelectorAll('[data-of]').find(n => n.getAttribute('data-of') === id && n.getAttribute('data-direction') === String(d));
const commitButton = () => takeover.querySelector('[data-action="enroll.commit"]');
const options = FIXTURE.preferenceOptions;
const LOC = 'ROM_LocationsDiscovered', MURDERS = options.find(o => o.label === 'Murders').editorID;

publish();
assert.equal(app.view.takeover, null);
assert.ok(!takeover.classList.contains('open'));

/* ---------- create: the ledger's New bond opens it; stamping ---------- */
for (const p of RM.parts) if (p.actions && p.actions['new-bond']) p.actions['new-bond'](element());
assert.deepEqual(plain(app.view.takeover), { mode: 'create', identity: null, candidate: null, staged: {}, query: '' });
assert.ok(takeover.classList.contains('open'));
assert.equal(takeover.querySelectorAll('[data-deed]').length, 58);
assert.equal(commitButton().disabled, true);
// The candidate rows' entrance plays on the first paint only; the notice clears the bar.
const html = page.document.documentElement;
assert.ok(html.classList.contains('enrolling'), 'the stage is marked while the takeover is open');
assert.ok(takeover.querySelector('.sheet').classList.contains('enter'), 'the first paint enters');
advance(1000);
assert.ok(!takeover.querySelector('.sheet').classList.contains('enter'), 'and the entrance ends');
const css = readFileSync(new URL('../../view/css/enrollment.css', import.meta.url), 'utf8');
for (const m of css.matchAll(/([^{}]+)\{[^}]*animation:/g)) {
    if (/\.cand\b/.test(m[1])) assert.match(m[1], /\.enter\b/, 'the candidate entrance is gated on .enter');
    if (/\.stamp\b/.test(m[1])) assert.match(m[1], /\.stamped\b/, 'the stamp pop is gated on .stamped');
}
assert.match(readFileSync(new URL('../../view/css/base.css', import.meta.url), 'utf8'), /html\.enrolling #notice \{ transform: translateY\(-\d+px\); \}/);
click(stamp(LOC, 1));
assert.deepEqual(plain(app.view.takeover.staged), { [LOC]: 1 }, 'the like stamp sets');
assert.equal(stamp(LOC, 1).getAttribute('aria-pressed'), 'true');
assert.equal(find('data-deed', LOC).getAttribute('data-staged'), '1');
click(stamp(LOC, 1));
assert.deepEqual(plain(app.view.takeover.staged), {}, 'clicking the active stamp clears it');
click(stamp(LOC, -1));
assert.deepEqual(plain(app.view.takeover.staged), { [LOC]: -1 }, 'the dislike stamp sets');
click(stamp(LOC, 1));
assert.deepEqual(plain(app.view.takeover.staged), { [LOC]: 1 }, 'like replaces dislike');
click(stamp(LOC, 0));
assert.deepEqual(plain(app.view.takeover.staged), {}, 'the neutral stamp clears');
// The clicked stamp keeps focus across the repaint.
stamp(MURDERS, -1).focus();
click(stamp(MURDERS, -1));
assert.equal(page.document.activeElement, stamp(MURDERS, -1), 'focus returns to the stamp');

// Enter on a focused row cycles neutral -> like -> favorite -> dislike -> pet peeve -> neutral; other keys leave it.
// The keys reach the page through pad.js's one listener on document (the takeover has none of its own).
topics.get('romantasyMode')({ mode: 'ledger' });
const keydown = page.document.listeners.keydown;
assert.equal(typeof keydown, 'function');
assert.equal(takeover.listeners.keydown, undefined, 'the takeover keeps no key listener of its own');
const key = k => { const ev = { key: k, target: page.document.activeElement, prevented: 0, preventDefault() { ev.prevented++; } }; keydown(ev); return ev; };
const locRow = () => find('data-deed', LOC);
locRow().focus();
assert.equal(key('Enter').prevented, 1);
assert.equal(app.view.takeover.staged[LOC], 1);
assert.equal(page.document.activeElement, locRow(), 'the row keeps focus');
key('Enter');
assert.equal(app.view.takeover.staged[LOC], 2);
key('Enter');
assert.equal(app.view.takeover.staged[LOC], -1);
key('Enter');
assert.equal(app.view.takeover.staged[LOC], -2);
key('Enter');
assert.equal(LOC in app.view.takeover.staged, false);
// Right from a deed row walks its five stamps, Favorite to Pet peeve, and Left walks back.
locRow().focus();
const walked = [];
for (let i = 0; i < 5; i++) {
    assert.equal(key('ArrowRight').prevented, 1);
    walked.push(page.document.activeElement.getAttribute('data-of') + ' ' + page.document.activeElement.getAttribute('data-direction'));
}
assert.deepEqual(walked, ['2', '1', '0', '-1', '-2'].map(d => LOC + ' ' + d), 'every stamp of the deed is reachable');
for (let i = 0; i < 4; i++) key('ArrowLeft');
assert.equal(page.document.activeElement, stamp(LOC, 2));
key('Enter');
assert.equal(app.view.takeover.staged[LOC], 2, 'Enter on the Favorite stamp sets favorite');
assert.equal(page.document.activeElement, stamp(LOC, 2), 'and it keeps focus');
click(stamp(LOC, 2));
assert.equal(LOC in app.view.takeover.staged, false, 'clicking the active Favorite clears it');
// Enter on a stamp runs that stamp's own action, once, as its click would.
stamp(LOC, 1).focus();
assert.equal(key('Enter').prevented, 1);
assert.equal(app.view.takeover.staged[LOC], 1, 'Enter on the like stamp sets like');
assert.equal(page.document.activeElement, stamp(LOC, 1), 'and the stamp keeps focus');
click(stamp(LOC, 1));
locRow().focus();
key('a');
assert.equal(LOC in app.view.takeover.staged, false);
click(stamp(LOC, 1));

// Commit stays inert until a candidate is chosen.
assert.equal(commitButton().disabled, true);
sent.length = 0;
for (const p of RM.parts) if (p.actions && p.actions['enroll.commit']) p.actions['enroll.commit'](commitButton());
assert.deepEqual(sent, [], 'commit sends nothing while canCommit is false');
click(find('data-candidate', '0001A6A6'));
assert.equal(app.view.takeover.candidate, '0001A6A6');
assert.ok(!takeover.querySelector('.sheet').classList.contains('enter'), 'a selection does not replay the entrance');
// Only the deed just stamped pops.
click(stamp(MURDERS, 1));
assert.deepEqual(takeover.querySelectorAll('.stamped').map(n => n.getAttribute('data-deed')), [MURDERS]);
click(stamp(MURDERS, -1));
assert.deepEqual(takeover.querySelectorAll('.stamped').map(n => n.getAttribute('data-deed')), [MURDERS]);
assert.equal(find('data-candidate', '0001A6A6').classList.contains('selected'), true);
assert.equal(commitButton().disabled, false);

/* ---------- a state refresh keeps stamps, query, choice, scroll, focus and caret ---------- */
const query = takeover.querySelector('.query');
query.value = 'o';
query.selectionStart = query.selectionEnd = 1;
takeover.listeners.input({ target: query });
assert.equal(app.view.takeover.query, 'o');
const deeds = takeover.querySelector('.deeds');
assert.ok(deeds.querySelectorAll('[data-deed]').length < 58, 'typing filters the list');
assert.equal(takeover.querySelector('.query'), query, 'typing repaints the deed list only');
deeds.scrollTop = 240;
query.focus();
publish({ message: 'Romance records synchronized.' });
assert.equal(takeover.querySelector('.query'), query, 'the search box survives a state');
assert.equal(page.document.activeElement, query, 'and keeps focus');
assert.equal(query.selectionStart, 1, 'and its caret');
assert.equal(takeover.querySelector('.deeds').scrollTop, 240, 'the deed list keeps its scroll');
assert.equal(app.view.takeover.query, 'o');
assert.equal(app.view.takeover.candidate, '0001A6A6');
assert.deepEqual(plain(app.view.takeover.staged), { [MURDERS]: -1, [LOC]: 1 });
// A state that changes the candidates repaints the list but keeps its scroll.
takeover.querySelector('.cands').scrollTop = 30;
publish({ enrollmentCandidates: [...FIXTURE.enrollmentCandidates, { name: 'Lydia', role: 'Nord · Housecarl', sourcePlugin: 'Skyrim.esm', referenceFormID: '000A2C94', baseFormID: '000A2C8E' }] });
assert.equal(takeover.querySelectorAll('[data-candidate]').length, 3);
assert.equal(takeover.querySelector('.cands').scrollTop, 30);
assert.equal(page.document.activeElement, query);
// Clearing the search shows every deed again.
query.value = '';
takeover.listeners.input({ target: query });
assert.equal(takeover.querySelectorAll('[data-deed]').length, 58);
query.value = '<zz>';
takeover.listeners.input({ target: query });
assert.ok(takeover.querySelector('.deeds').innerHTML.includes('No deeds match &quot;&lt;zz&gt;&quot;.'));
query.value = '';
takeover.listeners.input({ target: query });

/* ---------- the chosen candidate disappears: the choice drops, commit locks ---------- */
publish({ enrollmentCandidates: [FIXTURE.enrollmentCandidates[1]] });
assert.equal(app.view.takeover.candidate, null);
assert.deepEqual(plain(app.view.takeover.staged), { [MURDERS]: -1, [LOC]: 1 }, 'the stamps stay');
assert.equal(commitButton().disabled, true);
sent.length = 0;
for (const p of RM.parts) if (p.actions && p.actions['enroll.commit']) p.actions['enroll.commit'](commitButton());
assert.deepEqual(sent, []);
publish();

/* ---------- commit (create): a failed send keeps the sheet and the stamps ---------- */
click(find('data-candidate', '0001A6A6'));
sendResult = false;
sent.length = 0;
click(commitButton());
assert.equal(sent.length, 1);
assert.ok(app.view.takeover, 'a failed send leaves the takeover open');
assert.deepEqual(plain(app.view.takeover.staged), { [MURDERS]: -1, [LOC]: 1 });
assert.ok(takeover.classList.contains('open'));
sendResult = true;
sent.length = 0;
click(commitButton());
assert.equal(sent.length, 1);
const [name, payload] = sent[0];
assert.equal(name, 'romantasyBond');
assert.deepEqual(Object.keys(payload), ['kind', 'identity', 'preferences']);
assert.equal(payload.kind, 'enroll');
assert.equal(payload.identity, '0001A6A6');
assert.equal(payload.preferences.length, 58);
assert.ok(payload.preferences.every(p => Array.isArray(p) && p.length === 2 && typeof p[0] === 'string' && [1, 0, -1].includes(p[1])));
assert.deepEqual(payload.preferences.filter(p => p[1] !== 0).sort(), [[MURDERS, -1], [LOC, 1]].sort());
assert.deepEqual(payload.preferences.map(p => p[0]), plain(options.map(o => o.editorID)), 'one entry per deed, in order');
assert.equal(app.view.takeover, null, 'a sent page closes the takeover');
assert.ok(!page.document.documentElement.classList.contains('enrolling'), 'the notice returns to its place');
assert.ok(!takeover.classList.contains('open'));
assert.equal(takeover.innerHTML, '');

/* ---------- favorites and pet peeves: the cap, and what is sent ---------- */
app.emit('open-enroll', { mode: 'create' });
const ids = options.map(o => o.editorID);
for (const k of ids.slice(0, 3)) click(stamp(k, 2));
assert.deepEqual(plain(app.view.takeover.staged), { [ids[0]]: 2, [ids[1]]: 2, [ids[2]]: 2 });
assert.equal(stamp(ids[3], 2).disabled, true, 'a fourth Favorite stamp is disabled');
assert.equal(stamp(ids[3], 2).getAttribute('aria-disabled'), 'true');
assert.equal(stamp(ids[3], 2).getAttribute('title'), 'Three favorites at most.');
assert.equal(stamp(ids[0], 2).disabled, false, 'a staged favorite keeps its stamp');
assert.ok(takeover.querySelector('.ebar').innerHTML.includes('Three favorites at most.'));
click(stamp(ids[3], 2));
assert.equal(ids[3] in app.view.takeover.staged, false, 'a click on a disabled Favorite stamp changes nothing');
// Even when it reaches the action (a pad press on a stale control), the cap holds.
for (const p of RM.parts) if (p.actions && p.actions['enroll.stamp']) p.actions['enroll.stamp'](stamp(ids[3], 2));
assert.equal(ids[3] in app.view.takeover.staged, false, 'the action refuses a fourth favorite');
// With three favorites, the key cycle skips Favorite.
find('data-deed', ids[3]).focus();
key('Enter'); key('Enter');
assert.equal(app.view.takeover.staged[ids[3]], -1, 'like goes straight to dislike');
for (const k of ids.slice(4, 7)) click(stamp(k, -2));
assert.equal(stamp(ids[3], -2).disabled, true);
assert.equal(stamp(ids[3], -2).getAttribute('title'), 'Three pet peeves at most.');
key('Enter');
assert.equal(ids[3] in app.view.takeover.staged, false, 'dislike goes straight to neutral');
click(stamp(ids[1], 0));
assert.equal(stamp(ids[3], 2).disabled, false, 'freeing a favorite frees the stamp');
click(find('data-candidate', '0001A6A6'));
sent.length = 0;
click(commitButton());
const sealed = sent[0][1].preferences;
assert.ok(sealed.every(p => [2, 1, 0, -1, -2].includes(p[1])));
assert.deepEqual(sealed.filter(p => p[1] !== 0).sort(), [[ids[0], 2], [ids[2], 2], [ids[4], -2], [ids[5], -2], [ids[6], -2]].sort(), 'Favorite sends 2, Pet peeve -2');
assert.equal(app.view.takeover, null);
publish();

/* ---------- cancel sends nothing ---------- */
app.emit('open-enroll', { mode: 'create' });
click(stamp(LOC, 1));
sent.length = 0;
click(takeover.querySelector('[data-action="enroll.cancel"]'));
assert.deepEqual(sent, []);
assert.equal(app.view.takeover, null);
app.emit('open-enroll', { mode: 'create' });
assert.deepEqual(plain(app.view.takeover.staged), {}, 'a new takeover starts clean');
click(takeover.querySelector('[data-action="enroll.cancel"]'));

/* ---------- edit: existing stamps arrive; replace payload ---------- */
const inalion = FIXTURE.romances.find(r => r.name === 'Inalion');
const id = inalion.referenceFormID;
for (const p of RM.parts) if (p.actions && p.actions.edit) { const b = element(); b.setAttribute('data-target', id); p.actions.edit(b); }
const t = app.view.takeover;
assert.equal(t.mode, 'edit');
assert.equal(t.identity, id);
const expected = {};
for (const o of options) {
    if (inalion.likes.includes(o.label)) expected[o.editorID] = 1;
    else if (inalion.dislikes.includes(o.label)) expected[o.editorID] = -1;
}
assert.deepEqual(plain(t.staged), expected);
assert.ok(Object.keys(expected).length >= 3);
assert.match(takeover.innerHTML, /Seal personality/);
assert.ok(takeover.innerHTML.includes('<aside class="side"><h3 class="eyebrow">Companion</h3><div class="bond">') && takeover.innerHTML.includes('<b class="name">Inalion</b>'), 'the left column shows the bond being edited');
for (const [k, d] of Object.entries(expected)) assert.equal(stamp(k, d).getAttribute('aria-pressed'), 'true');
assert.equal(commitButton().disabled, false);
assert.equal(takeover.querySelectorAll('[data-candidate]').length, 0, 'no candidates in edit mode');
click(stamp(LOC, -1));
sent.length = 0;
click(commitButton());
assert.equal(sent[0][1].kind, 'replace');
assert.equal(sent[0][1].identity, id);
assert.equal(sent[0][1].preferences.length, 58);
assert.deepEqual(sent[0][1].preferences.find(p => p[0] === LOC), [LOC, -1]);
assert.equal(app.view.takeover, null);

/* ---------- edit: the bond disappears while open ---------- */
app.emit('open-enroll', { mode: 'edit', identity: id });
assert.ok(app.view.takeover);
sent.length = 0;
publish({ romances: FIXTURE.romances.filter(r => r.referenceFormID !== id) });
assert.equal(app.view.takeover, null, 'the takeover closes');
assert.ok(!takeover.classList.contains('open'));
assert.deepEqual(sent, [], 'and sends nothing');
app.emit('open-enroll', { mode: 'edit', identity: id });
assert.equal(app.view.takeover, null, 'a bond that is not there cannot be edited');
publish();

/* ---------- names with markup and other alphabets; missing keys ---------- */
const hostile = '"><img src=x onerror=1>\'';
publish({ enrollmentCandidates: [{ name: 'Серана <b>' + hostile, role: hostile, sourcePlugin: '', referenceFormID: '0001A6A6', baseFormID: '' }] });
app.emit('open-enroll', { mode: 'create' });
assert.ok(takeover.innerHTML.includes('Серана &lt;b&gt;'));
assert.ok(!takeover.innerHTML.includes('<b>') && !takeover.innerHTML.includes('<img src=x'));
click(takeover.querySelector('[data-action="enroll.cancel"]'));
const noKeys = plain(FIXTURE);
delete noKeys.enrollmentCandidates;
delete noKeys.preferenceOptions;
topics.get('romantasyState')({ ...noKeys, revision: ++revision });
app.emit('open-enroll', { mode: 'create' });
assert.match(takeover.innerHTML, /No companions are available to add right now\. Recruit a follower, then try again\./);
assert.match(takeover.innerHTML, /No deeds are available\./);
assert.equal(commitButton().disabled, true);
click(takeover.querySelector('[data-action="enroll.cancel"]'));
app.emit('open-enroll', { mode: 'edit', identity: id });
assert.match(takeover.innerHTML, /No deeds are available\./);
assert.deepEqual(plain(app.view.takeover.staged), {});
assert.equal(commitButton().disabled, true);
click(takeover.querySelector('[data-action="enroll.cancel"]'));
publish();

/* ---------- closing the ledger drops the takeover once __uimode stays off ---------- */
app.emit('open-enroll', { mode: 'create' });
click(stamp(LOC, 1));
RM.bridge.focused = false;
topics.get('__uimode')(false);
advance(100);
assert.ok(app.view.takeover, 'a blink keeps it');
RM.bridge.focused = true;
topics.get('__uimode')(true);
advance(1000);
assert.ok(app.view.takeover, 'still there after the blink');
RM.bridge.focused = false;
topics.get('__uimode')(false);
advance(250);
assert.equal(app.view.takeover, null, 'the takeover is cleared');
assert.ok(!takeover.classList.contains('open'));
assert.equal(takeover.innerHTML, '');
RM.bridge.focused = true;
topics.get('__uimode')(true);
publish();
assert.equal(app.view.takeover, null, 'and does not come back on the next open');

assert.equal(typeof e.open, 'function'); assert.equal(typeof e.commit, 'function');
assert.equal(typeof e.cancel, 'function'); assert.equal(typeof e.render, 'function');
console.log('EnrollmentFlow: ok');
