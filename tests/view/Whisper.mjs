// The whisper card: its own page (view/whisper.html) in a click-through HUD view. The pure markup and
// sequence rules, the page's link to the host with a fake clock, the page file and its stylesheet, and
// the browser harness (whisper.html?mock).
import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const VIEW = new URL('../../view/', import.meta.url);
const read = path => readFileSync(new URL(path, VIEW), 'utf8');
const whisper = createRequire(import.meta.url)('../../view/js/whisper.js');

/* ---------- pure: the card ---------- */
assert.equal(whisper.HOLD_MS, 4000);
assert.equal(whisper.ENTER_MS, 180);
assert.equal(whisper.LEAVE_MS, 220);

const gain = { name: 'Aelira', line: 'Dungeons Cleared · +10', tone: 'gain', icon: 'star', seq: 1 };
const card = whisper.cardHtml(gain);
assert.match(card, /class="whisper gain"/, 'the tone is the card\'s class');
assert.ok(card.includes('src="icons/star.png"'), 'the icon path');
assert.ok(card.includes('>Aelira<') && card.includes('>Dungeons Cleared · +10<'), 'the name and the line');
assert.ok(card.indexOf('Aelira') < card.indexOf('Dungeons Cleared'), 'the name above the line');
assert.match(whisper.cardHtml({ ...gain, tone: 'loss' }), /class="whisper loss"/);
assert.match(whisper.cardHtml({ ...gain, tone: 'plain' }), /class="whisper plain"/);
for (const tone of ['sideways', '', null, 5, 'GAIN', undefined]) {
    assert.match(whisper.cardHtml({ ...gain, tone }), /class="whisper plain"/, 'an unknown tone is plain: ' + tone);
}
for (const icon of ['star', 'heart', 'heartbroken', 'dislikes', 'neutral']) {
    assert.ok(whisper.cardHtml({ ...gain, icon }).includes('src="icons/' + icon + '.png"'), icon + ' is used as named');
    assert.ok(existsSync(new URL('icons/' + icon + '.png', VIEW)), 'icons/' + icon + '.png exists');
}
for (const icon of ['crown', '', null, 7, '../index', 'star.png', 'Heart']) {
    assert.ok(whisper.cardHtml({ ...gain, icon }).includes('src="icons/neutral.png"'), 'an unknown icon falls back to neutral: ' + icon);
}
assert.equal(whisper.FALLBACK_ICON, 'neutral');
assert.ok(existsSync(new URL('icons/' + whisper.FALLBACK_ICON + '.png', VIEW)), 'the fallback icon exists');

// Text comes from other mods: everything is escaped.
const hostile = whisper.cardHtml({ ...gain, name: '<b>Lydia</b>', line: '<img src=x onerror=alert(1)> & "q" \'s' });
assert.ok(hostile.includes('&lt;b&gt;Lydia&lt;/b&gt;') && !hostile.includes('<b>'), '<b> shows as text');
assert.ok(!hostile.includes('<img src=x'), 'no raw markup from the line');
assert.ok(hostile.includes('&amp;') && hostile.includes('&quot;q&quot;') && hostile.includes('&#39;s'));
assert.ok(!/<b>|onerror=/.test(whisper.cardHtml({ name: '<b>x</b>', line: 'y', tone: '<b>', icon: '"><b onerror=1' })), 'tone and icon cannot inject either');
for (const raw of [{}, null, undefined, 'x', 42, [], { name: 5, line: {} }]) assert.equal(typeof whisper.cardHtml(raw), 'string');

// Nothing focusable, ever.
const FOCUSABLE = /<(a|button|input|select|textarea|iframe)\b|tabindex|contenteditable|autofocus/i;
assert.doesNotMatch(card, FOCUSABLE, 'the card has no focusable element');

// seq: an integer from 1 up is echoed; anything else is 0.
assert.equal(whisper.normalize({ seq: 7 }).seq, 7);
for (const seq of [undefined, null, 0, -3, 1.5, '12', NaN, Infinity, 2147483648]) assert.equal(whisper.normalize({ seq }).seq, 0, 'seq ' + String(seq) + ' is 0');

/* ---------- a fake clock ---------- */
let now = 0, nextTimer = 1;
const timers = new Map();
const setTimeoutFake = (fn, ms) => { const id = nextTimer++; timers.set(id, { at: now + (ms || 0), fn }); return id; };
const clearTimeoutFake = id => { timers.delete(id); };
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

/* ---------- pure: the sequence ---------- */
const done = [], phases = [];
let painted = '';
const player = whisper.createPlayer({
    setTimeout: setTimeoutFake, clearTimeout: clearTimeoutFake,
    paint: html => { painted = html; }, phase: p => { phases.push(p); }, clear: () => { painted = ''; phases.push(''); },
    done: seq => done.push(seq)
});

player.show(gain);
assert.ok(painted.includes('Aelira'), 'painted at once');
assert.deepEqual(phases, ['in'], 'entering');
advance(whisper.HOLD_MS - 1);
assert.deepEqual(phases, ['in'], 'still up at 3 999 ms');
advance(1);
assert.deepEqual(phases, ['in', 'out'], 'leaving at 4 000 ms');
assert.deepEqual(done, [], 'done waits for the leave');
advance(whisper.LEAVE_MS - 1);
assert.deepEqual(done, []);
advance(1);
assert.deepEqual(done, [1], 'done 4 000 + 220 ms after showing');
assert.equal(painted, '', 'and the card is gone');
advance(60000);
assert.deepEqual(done, [1], 'exactly once');

// A second whisper while one is up replaces it: the first is answered at once, the second when its own time ends.
done.length = 0; phases.length = 0;
player.show({ ...gain, seq: 2 });
advance(1000);
player.show({ ...gain, name: 'Serana', tone: 'loss', seq: 3 });
assert.deepEqual(done, [2], 'the replaced whisper is answered at once');
assert.ok(painted.includes('Serana'));
advance(whisper.HOLD_MS + whisper.LEAVE_MS - 1);
assert.deepEqual(done, [2]);
advance(1);
assert.deepEqual(done, [2, 3], 'the replacement has its own full time');
advance(60000);
assert.deepEqual(done, [2, 3], 'one done per whisper');

// Replaced while leaving: the same.
done.length = 0;
player.show({ ...gain, seq: 4 });
advance(whisper.HOLD_MS + 100);
player.show({ ...gain, seq: 5 });
assert.deepEqual(done, [4]);
advance(whisper.HOLD_MS + whisper.LEAVE_MS);
assert.deepEqual(done, [4, 5]);
advance(60000);
assert.deepEqual(done, [4, 5]);

// The same seq sent again while it is up changes nothing: no second done, its time runs on.
done.length = 0;
player.show({ ...gain, seq: 6 });
advance(3000);
player.show({ ...gain, seq: 6 });
advance(whisper.HOLD_MS + whisper.LEAVE_MS - 3000);
assert.deepEqual(done, [6]);
advance(60000);
assert.deepEqual(done, [6]);

// No seq: still shown, answered with 0; two in a row are two whispers.
done.length = 0;
player.show({ name: 'Inalion', line: 'Glad to have you back', tone: 'gain', icon: 'heart' });
assert.ok(painted.includes('Inalion'));
player.show({ name: 'Inalion', line: 'Feels warm toward you', tone: 'gain', icon: 'heart', seq: 'x' });
assert.deepEqual(done, [0]);
advance(whisper.HOLD_MS + whisper.LEAVE_MS);
assert.deepEqual(done, [0, 0]);

// Nothing to say (no name and no line, or not an object): not shown, answered at once, and a whisper on screen is left alone.
done.length = 0;
player.show({ ...gain, seq: 8 });
for (const raw of [null, 'x', 42, [], {}, { seq: 9 }, { name: '', line: '' }]) player.show(raw);
assert.deepEqual(done, [0, 0, 0, 0, 0, 9, 0]);
assert.ok(painted.includes('Aelira'), 'the whisper on screen stays');
advance(whisper.HOLD_MS + whisper.LEAVE_MS);
assert.deepEqual(done, [0, 0, 0, 0, 0, 9, 0, 8]);

console.log('Whisper (pure): ok');

/* ---------- the page script in a vm: the host link ---------- */
function element(tag) {
    const attrs = {};
    return {
        tagName: (tag || 'div').toUpperCase(), innerHTML: '', className: '', textContent: '', style: {}, attrs,
        classList: { add() {}, remove() {}, contains: () => false },
        setAttribute: (k, v) => { attrs[k] = String(v); }, getAttribute: k => (k in attrs ? attrs[k] : null),
        removeAttribute: k => { delete attrs[k]; }, appendChild() {}, addEventListener() {}
    };
}
function pageBox(search, host) {
    const card = element();
    const box = {
        console: { log() {}, warn() {}, error() {} }, setTimeout: setTimeoutFake, clearTimeout: clearTimeoutFake,
        location: { search },
        document: {
            readyState: 'complete', documentElement: element('html'), head: element('head'), body: element('body'),
            getElementById: id => (id === 'whisper' ? card : null), createElement: element, addEventListener() {}
        }
    };
    box.window = box;
    if (host) box.magelight = host;
    box.card = card;
    return box;
}
const runIn = (box, path) => vm.runInNewContext(read(path), box, { filename: path });

const hostSent = [], subs = new Map();
const host = {
    on: (channel, fn) => { subs.set(channel, fn); },
    send: (name, json) => hostSent.push([name, JSON.parse(json)])
};
const box = pageBox('', host);
runIn(box, 'js/format.js');
runIn(box, 'js/whisper.js');
assert.equal(typeof subs.get('romantasyWhisper'), 'function', 'the page listens for romantasyWhisper on the host');
subs.get('romantasyWhisper')(JSON.stringify({ name: 'Aelira', line: 'Reached Lover', tone: 'gain', icon: 'heart', seq: 11 }));
assert.match(box.card.innerHTML, /Reached Lover/, 'the card is painted into #whisper');
assert.equal(box.card.className, 'in');
advance(whisper.HOLD_MS);
assert.equal(box.card.className, 'out');
advance(whisper.LEAVE_MS);
assert.equal(box.card.innerHTML, '');
assert.equal(box.card.className, '');
assert.deepEqual(hostSent, [['romantasyWhisperDone', { seq: 11 }]], 'the page answers romantasyWhisperDone with the seq');
subs.get('romantasyWhisper')('not json');
assert.deepEqual(hostSent[1], ['romantasyWhisperDone', { seq: 0 }], 'a payload that does not parse is answered at once');
subs.get('romantasyWhisper')(JSON.stringify({ name: 'A', line: 'B', seq: 12 }));
subs.get('romantasyWhisper')(JSON.stringify({ name: 'C', line: 'D', seq: 13 }));
assert.deepEqual(hostSent[2], ['romantasyWhisperDone', { seq: 12 }]);
advance(whisper.HOLD_MS + whisper.LEAVE_MS);
assert.deepEqual(hostSent[3], ['romantasyWhisperDone', { seq: 13 }]);
assert.equal(hostSent.length, 4);
// A second connect does not double the listener.
assert.equal(box.RM.whisper.connect(host), false);

// A send that throws while a newer whisper replaces the one up: the new one is still painted and answered once.
const flakySent = [], flakySubs = new Map();
let throwOnce = true;
const flaky = {
    on: (channel, fn) => { flakySubs.set(channel, fn); },
    send: (name, json) => {
        if (throwOnce) { throwOnce = false; throw new Error('host gone for a moment'); }
        flakySent.push([name, JSON.parse(json)]);
    }
};
const flakyBox = pageBox('', flaky);
runIn(flakyBox, 'js/format.js');
runIn(flakyBox, 'js/whisper.js');
flakySubs.get('romantasyWhisper')(JSON.stringify({ name: 'A', line: 'First', seq: 21 }));
assert.doesNotThrow(() => flakySubs.get('romantasyWhisper')(JSON.stringify({ name: 'B', line: 'Second', seq: 22 })));
assert.match(flakyBox.card.innerHTML, /Second/, 'the new whisper is painted though the answer for the old one threw');
assert.equal(flakyBox.card.className, 'in');
advance(whisper.HOLD_MS + whisper.LEAVE_MS);
assert.deepEqual(flakySent, [['romantasyWhisperDone', { seq: 22 }]], 'the new whisper is answered once');

// Stronger contrast and reduced motion: two optional fields of each payload, put on the page's root (tokens.css and
// whisper.css answer them). Only the exact strings count; anything else, or no field, is the normal look.
assert.deepEqual(whisper.comfort({ contrast: 'high', motion: 'reduced' }), { contrast: true, motion: true });
assert.deepEqual(whisper.comfort({}), { contrast: false, motion: false });
for (const v of [1, {}, [], 'HIGH', 'REDUCED', 'high ', true, null, 'reduced', 'normal']) {
    assert.equal(whisper.comfort({ contrast: v }).contrast, v === 'high', 'contrast ' + JSON.stringify(v));
}
for (const v of [1, {}, [], 'REDUCED', 'reduce', true, null, 'high']) assert.equal(whisper.comfort({ motion: v }).motion, false, 'motion ' + JSON.stringify(v));
for (const raw of [null, undefined, 'x', 42, []]) assert.deepEqual(whisper.comfort(raw), { contrast: false, motion: false });
const rootOf = b => b.document.documentElement;
const look = () => [rootOf(box).getAttribute('data-contrast'), rootOf(box).getAttribute('data-motion')];
subs.get('romantasyWhisper')(JSON.stringify({ name: 'A', line: 'B', seq: 31, contrast: 'high', motion: 'reduced' }));
assert.deepEqual(look(), ['high', 'reduced'], 'both on: both attributes on the root');
subs.get('romantasyWhisper')(JSON.stringify({ name: 'A', line: 'B', seq: 32, contrast: 'high' }));
assert.deepEqual(look(), ['high', null], 'a field left out removes its attribute');
subs.get('romantasyWhisper')(JSON.stringify({ name: 'A', line: 'B', seq: 33 }));
assert.deepEqual(look(), [null, null], 'no fields: the normal look');
for (const [contrast, motion] of [[5, { a: 1 }], ['HIGH', 'REDUCED'], [{}, 1]]) {
    subs.get('romantasyWhisper')(JSON.stringify({ name: 'A', line: 'B', seq: 34, contrast: 'high', motion: 'reduced' }));
    subs.get('romantasyWhisper')(JSON.stringify({ name: 'A', line: 'B', seq: 35, contrast, motion }));
    assert.deepEqual(look(), [null, null], 'hostile values are the normal look: ' + JSON.stringify([contrast, motion]));
}
assert.ok(!/data-contrast|data-motion/.test(box.card.innerHTML), 'the card itself carries neither');
advance(whisper.HOLD_MS + whisper.LEAVE_MS);
hostSent.length = 0;

// Without a host the page waits quietly until one is attached.
const lonely = pageBox('');
runIn(lonely, 'js/format.js');
assert.doesNotThrow(() => runIn(lonely, 'js/whisper.js'));
assert.equal(typeof lonely.RM.whisper.connect, 'function');

console.log('Whisper (page): ok');

/* ---------- whisper.html ---------- */
const html = read('whisper.html');
const refs = [...html.matchAll(/\s(?:src|href)="([^"]+)"/g)].map(m => m[1]);
for (const ref of refs) {
    assert.ok(!/^[a-z]+:|^\/\//i.test(ref) && !ref.startsWith('/'), 'whisper.html loads only local files: ' + ref);
    assert.ok(existsSync(new URL(ref, VIEW)), 'whisper.html names ' + ref + ', which exists');
}
assert.deepEqual(refs, ['css/tokens.css', 'css/whisper.css', 'js/strings.js', 'js/format.js', 'js/whisper.js'], 'the tokens, its stylesheet, the page words, the escape helper and its script');
assert.doesNotMatch(html, /https?:\/\//, 'no remote address anywhere');
assert.doesNotMatch(html, FOCUSABLE, 'whisper.html has no focusable element');
assert.equal((html.match(/\sid="whisper"/g) || []).length, 1, 'one card host');
const bodyMarkup = html.slice(html.indexOf('<body'), html.indexOf('</body>')).replace(/<script[\s\S]*?<\/script>/g, '');
assert.equal([...bodyMarkup.matchAll(/<([a-z][a-z0-9]*)\b/g)].map(m => m[1]).join(','), 'body,div', 'the body holds only the card host');

/* ---------- whisper.css ---------- */
function rules(text) {
    text = text.replace(/\/\*[\s\S]*?\*\//g, '');
    const out = [];
    let i = 0;
    while (i < text.length) {
        const open = text.indexOf('{', i);
        if (open < 0) break;
        const head = text.slice(i, open).trim();
        let depth = 1, j = open + 1;
        while (depth > 0 && j < text.length) { if (text[j] === '{') depth++; else if (text[j] === '}') depth--; j++; }
        const body = text.slice(open + 1, j - 1);
        if (head.startsWith('@media')) out.push(...rules(body).map(r => ({ ...r, media: head })));
        else if (!head.startsWith('@')) {
            const decls = {};
            for (const d of body.split(';')) { const k = d.indexOf(':'); if (k > 0) decls[d.slice(0, k).trim()] = d.slice(k + 1).trim(); }
            out.push({ selectors: head.split(',').map(s => s.trim().replace(/\s+/g, ' ')), decls });
        }
        i = j;
    }
    return out;
}
const cssText = read('css/whisper.css');
const css = rules(cssText);
const decls = selector => Object.assign({}, ...css.filter(r => !r.media && r.selectors.includes(selector)).map(r => r.decls));
assert.equal(decls('html').overflow, 'hidden', 'html never scrolls');
assert.equal(decls('body').overflow, 'hidden', 'body never scrolls');
assert.equal(decls('body').background, 'transparent', 'the body is fully transparent');
assert.equal(decls('body')['pointer-events'], 'none', 'the page takes no pointer');
assert.equal(decls('.whisper').width, '420px', 'the card is the view\'s width');
assert.equal(decls('.whisper').height, '96px', 'and its height, whatever the text');
assert.equal(decls('.whisper').overflow, 'hidden');
assert.equal(decls('.whisper .icon').width, '48px');
assert.equal(decls('.whisper .icon').height, '48px');
for (const part of ['.whisper .name', '.whisper .line']) {
    const d = decls(part);
    assert.equal(d['white-space'], 'nowrap', part + ' stays on one line');
    assert.equal(d.overflow, 'hidden', part + ' clips');
    assert.equal(d['text-overflow'], 'ellipsis', part + ' ends in an ellipsis');
}
assert.equal(decls('.whisper .words')['min-width'], '0', 'the words may shrink below their text');
assert.equal(decls('.whisper.gain')['border-color'], 'var(--rosegold)', 'gold edge for gain');
assert.equal(decls('.whisper.loss')['border-color'], 'var(--rose)', 'rose edge for loss');
assert.equal(decls('.whisper.plain')['border-color'], 'var(--glass-edge)', 'the hairline for plain');
assert.equal(decls('.whisper').background, 'var(--whisper-plate)', 'the dark plate');
assert.match(read('css/tokens.css'), /--whisper-plate:\s*rgba\(16, 3, 10, \.82\);/, 'the plate token is the darkest wine at 82%');
const cssCode = cssText.replace(/\/\*[\s\S]*?\*\//g, '');
assert.doesNotMatch(cssCode, /#[0-9a-f]{3,8}\b|rgba?\(|hsla?\(/i, 'colours come from the tokens');
assert.doesNotMatch(cssCode, /gradient/, 'no gradient under the rounded corners');
assert.doesNotMatch(cssCode, /cursor\s*:\s*pointer/);
assert.doesNotMatch(cssCode, /(^|[\s;{])gap\s*:/, 'margins, not flex gap');
assert.doesNotMatch(cssCode, /scrollbar|overflow(-[xy])?\s*:\s*(auto|scroll)/, 'nothing scrolls');
// The motion: 180 ms in with an 8px rise, 220 ms out; reduced motion keeps the fade and drops the rise.
assert.match(decls('#whisper.in .whisper').animation || '', /^whisper-in 180ms\b/);
assert.match(decls('#whisper.out .whisper').animation || '', /^whisper-out 220ms\b/);
assert.match(cssText, /@keyframes whisper-in\s*\{\s*from\s*\{\s*opacity:\s*0;\s*transform:\s*translateY\(8px\);\s*\}/);
const calm = css.filter(r => r.media && /prefers-reduced-motion:\s*reduce/.test(r.media));
assert.ok(calm.some(r => r.selectors.includes('#whisper.in .whisper') && /^whisper-fade 180ms\b/.test(r.decls.animation || '')), 'reduced motion only fades');
assert.match(cssText, /@keyframes whisper-fade\s*\{\s*from\s*\{\s*opacity:\s*0;\s*\}\s*\}/);
assert.ok(css.some(r => r.selectors.includes('#whisper.out .whisper') && r.decls.opacity === '0'), 'leaving ends hidden even if no animation runs');
// The Reduce motion setting does what the system setting does: the card only fades in.
assert.match(decls('html[data-motion="reduced"] #whisper.in .whisper').animation || '', /^whisper-fade 180ms\b/, 'reduced motion by the setting only fades');
// Stronger contrast: the plate is solid and the edge a thicker token line.
assert.equal(decls('.whisper').border, 'var(--hairline) solid var(--glass-edge)', 'the edge follows the hairline token');
assert.match(read('css/tokens.css'), /html\[data-contrast="high"\] \{[^}]*--whisper-plate:\s*rgb\(16, 3, 10\);/, 'the plate is solid under stronger contrast');

console.log('Whisper (page files): ok');

/* ---------- the harness: whisper.html?mock cycles one whisper of each tone ---------- */
assert.match(html, /\[\?&\]mock\\b/, 'whisper.html loads the harness only with ?mock');
assert.match(html, /dev\/whisper-mock\.js/);
const mockBox = pageBox('?mock');
runIn(mockBox, 'js/format.js');
runIn(mockBox, 'js/whisper.js');
runIn(mockBox, 'dev/whisper-mock.js');
const seen = [];
mockBox.magelight.on('romantasyWhisper', raw => seen.push(JSON.parse(raw)));
const mock = mockBox.__whisperMock;
assert.ok(mock.log.length === 0 || mock.log.every(e => e.name === 'romantasyWhisperDone'));
// The first whisper went out at load (before our listener); the page painted it.
assert.ok(mockBox.card.innerHTML.includes('whisper gain'), 'the first is a gain');
for (let i = 0; i < 8; i++) advance(whisper.HOLD_MS + whisper.LEAVE_MS + mock.PAUSE_MS);
const tones = [mock.samples[0].tone].concat(seen.map(w => w.tone));
assert.deepEqual(tones.slice(0, 3), ['gain', 'loss', 'plain'], 'one of each tone, in turn');
assert.deepEqual([...new Set(mock.samples.map(s => s.icon))].sort(), ['dislikes', 'heart', 'heartbroken', 'neutral', 'star'], 'every icon the plugin sends');
assert.equal(seen.length, 8, 'it keeps cycling');
const seqs = [1].concat(seen.map(w => w.seq));
assert.deepEqual(seqs, seqs.map((_, i) => i + 1), 'each with the next seq');
assert.deepEqual(JSON.parse(JSON.stringify(mock.log.slice(0, 3).map(e => e.payload))), [{ seq: 1 }, { seq: 2 }, { seq: 3 }], 'the page\'s answers are logged');
// &long: every name and line is far too long for the card.
const longBox = pageBox('?mock&long');
runIn(longBox, 'js/format.js');
runIn(longBox, 'js/whisper.js');
runIn(longBox, 'dev/whisper-mock.js');
assert.ok(longBox.__whisperMock.samples.every(s => s.name.length > 60 && s.line.length > 80));

console.log('Whisper: ok');
