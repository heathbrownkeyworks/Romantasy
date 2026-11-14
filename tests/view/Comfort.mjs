// The three comfort settings: Ledger size, Stronger contrast and Reduce motion. The page puts them on the root
// (data-size, data-contrast="high", data-motion="reduced") from every state and re-runs the fit only when the size
// changes; the stylesheets answer the two attributes: every long or looping animation stops under reduced motion,
// and under stronger contrast the tokens make text bright and panels solid (measured here).
import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import vm from 'node:vm';

const VIEW = new URL('../../view/', import.meta.url);
const read = path => readFileSync(new URL(path, VIEW), 'utf8');

/* ---------- the root attributes, from every state ---------- */
function element() {
    const attrs = {}, classes = new Set();
    return {
        innerHTML: '', className: '', textContent: '', value: '', style: {}, offsetWidth: 0, isConnected: true, disabled: false, attrs,
        classList: { add: c => classes.add(c), remove: c => classes.delete(c), toggle: (c, on) => (on ?? !classes.has(c)) ? classes.add(c) : classes.delete(c), contains: c => classes.has(c) },
        setAttribute: (k, v) => { attrs[k] = String(v); }, getAttribute: k => (k in attrs ? attrs[k] : null), removeAttribute: k => { delete attrs[k]; },
        querySelector: () => null, querySelectorAll: () => [], addEventListener() {}, focus() {}, appendChild() {}, remove() {}, insertAdjacentHTML() {}
    };
}
// A fresh page: its root, the bridge topics it listens on and the fits it asked for.
function makePage() {
    const elements = new Map();
    const topics = new Map();
    const fits = [];
    const root = element();
    const page = {
        console, setTimeout, clearTimeout,
        document: {
            readyState: 'complete', documentElement: root,
            getElementById: id => { if (!elements.has(id)) elements.set(id, element()); return elements.get(id); },
            addEventListener() {}, createElement: element, querySelector: () => null
        }
    };
    page.window = page;
    page.RM = {
        fit: { apply: size => fits.push(size) },
        bridge: { focused: true, on: (topic, fn) => topics.set(topic, fn), send: () => true, sound() {} }
    };
    for (const file of ['strings.js', 'tiers.js', 'format.js', 'state.js', 'notice.js', 'roster.js', 'ledger.js', 'settings.js', 'modals.js', 'app.js']) {
        vm.runInNewContext(read('js/' + file), page, { filename: file });
    }
    return { root, topics, fits };
}
const { root, topics, fits } = makePage();
const fixture = { window: {} };
vm.runInNewContext(read('dev/fixture.js'), fixture);
const base = JSON.parse(JSON.stringify(fixture.window.ROMANTASY_FIXTURE));
const publish = extra => topics.get('romantasyState')({ ...base, ...extra });
const attrs = () => ({ size: root.getAttribute('data-size'), contrast: root.getAttribute('data-contrast'), motion: root.getAttribute('data-motion') });

fits.length = 0;
publish({});
assert.deepEqual(attrs(), { size: 'standard', contrast: null, motion: null }, 'the defaults: standard size, no contrast, no reduced motion');
publish({ ledgerSize: 'largest', highContrast: true, reduceMotion: true });
assert.deepEqual(attrs(), { size: 'largest', contrast: 'high', motion: 'reduced' }, 'each setting on the root');
assert.deepEqual(fits, ['largest'], 'the fit runs again when the size changes');
publish({ ledgerSize: 'largest', highContrast: true, reduceMotion: true, message: 'again' });
assert.deepEqual(fits, ['largest'], 'and not when it does not');
publish({ ledgerSize: 'large', highContrast: false, reduceMotion: true });
assert.deepEqual(attrs(), { size: 'large', contrast: null, motion: 'reduced' }, 'a setting turned off removes its attribute');
publish({ ledgerSize: 'large', highContrast: true, reduceMotion: false });
assert.deepEqual(attrs(), { size: 'large', contrast: 'high', motion: null });
// Hostile values normalise to the defaults (state.js): the size to standard, the switches to off.
publish({ ledgerSize: 'HUGE', highContrast: 'yes', reduceMotion: 1 });
assert.deepEqual(attrs(), { size: 'standard', contrast: null, motion: null });
assert.deepEqual(fits, ['largest', 'large', 'standard'], 'one fit per change of size');

// A tier popup before any state (the plugin sends no state outside the ledger): the mode carries the settings.
{
    const fresh = makePage();
    const look = () => ({ size: fresh.root.getAttribute('data-size'), contrast: fresh.root.getAttribute('data-contrast'), motion: fresh.root.getAttribute('data-motion') });
    fresh.topics.get('romantasyMode')({ mode: 'popup', ledgerSize: 'largest', highContrast: true, reduceMotion: true });
    assert.deepEqual(look(), { size: 'largest', contrast: 'high', motion: 'reduced' }, 'a popup with no state before it follows the comfort settings in its mode');
    assert.deepEqual(fresh.fits.filter(Boolean), ['largest'], 'and the stage is fitted to its size (after the boot fit)');
    fresh.topics.get('romantasyMode')({ mode: 'popup' });
    assert.deepEqual(look(), { size: 'largest', contrast: 'high', motion: 'reduced' }, 'a mode message without them changes nothing');
    fresh.topics.get('romantasyMode')({ mode: 'popup', ledgerSize: 'standard', highContrast: false, reduceMotion: false });
    assert.deepEqual(look(), { size: 'standard', contrast: null, motion: null }, 'turned off in the mode, they come off');
}

console.log('Comfort (root): ok');

/* ---------- the stylesheets ---------- */
function rules(file) {
    const text = read('css/' + file).replace(/\/\*[\s\S]*?\*\//g, '');
    const out = [];
    let i = 0;
    while (i < text.length) {
        const open = text.indexOf('{', i);
        if (open < 0) break;
        const head = text.slice(i, open).trim();
        let depth = 1, j = open + 1;
        while (depth > 0 && j < text.length) { if (text[j] === '{') depth++; else if (text[j] === '}') depth--; j++; }
        const body = text.slice(open + 1, j - 1);
        if (!head.startsWith('@')) {
            const decls = {};
            for (const d of body.split(';')) { const k = d.indexOf(':'); if (k > 0) decls[d.slice(0, k).trim()] = d.slice(k + 1).trim(); }
            out.push({ file, selectors: head.split(',').map(s => s.trim().replace(/\s+/g, ' ')), decls });
        }
        i = j;
    }
    return out;
}
const FILES = readdirSync(new URL('css/', VIEW)).filter(f => f.endsWith('.css') && read('index.html').includes('href="css/' + f + '"'));
const ALL = FILES.flatMap(rules);
const decls = selector => Object.assign({}, ...ALL.filter(r => r.selectors.includes(selector)).map(r => r.decls));

// Split a list value at its top-level commas (not inside cubic-bezier(...) or calc(...)).
function parts(value) {
    const out = [];
    let depth = 0, start = 0;
    for (let i = 0; i < value.length; i++) {
        if (value[i] === '(') depth++;
        else if (value[i] === ')') depth--;
        else if (value[i] === ',' && depth === 0) { out.push(value.slice(start, i).trim()); start = i + 1; }
    }
    out.push(value.slice(start).trim());
    return out;
}
// One animation or transition: its duration in ms (Infinity when it is not a plain time), whether it loops, its fill.
function timing(part) {
    const words = part.replace(/\([^()]*\)/g, '()').split(/\s+/);
    const time = words.find(w => /^\d*\.?\d+m?s$/.test(w));
    const ms = !time ? (/calc|var/.test(part) ? Infinity : 0) : time.endsWith('ms') ? parseFloat(time) : parseFloat(time) * 1000;
    return { ms, loops: words.includes('infinite'), fill: words.some(w => ['both', 'backwards', 'forwards'].includes(w)) };
}
const LONG_MS = 400;
const isLong = value => value !== 'none' && parts(value).some(p => { const t = timing(p); return t.loops || t.ms > LONG_MS; });
const REDUCED = 'html[data-motion="reduced"] ';

// Every animation in the ledger's stylesheets that loops or runs longer than 400 ms is answered, for every selector
// that carries it, by a reduced rule: none, or a short fade that does not loop and has no fill mode (so an element that
// enters by it ends visible, and rests visible even where the motion never runs).
const answered = [];
for (const r of ALL) {
    for (const property of ['animation', 'transition']) {
        const value = r.decls[property];
        if (!value || !isLong(value)) continue;
        for (const selector of r.selectors) {
            if (selector.startsWith('html[data-motion')) continue;
            const reduced = decls(REDUCED + selector)[property];
            assert.ok(reduced, `${r.file}: ${selector} { ${property}: ${value} } has no reduced-motion rule`);
            assert.ok(!isLong(reduced), `${r.file}: under reduced motion ${selector} still runs ${reduced}`);
            if (reduced !== 'none') for (const p of parts(reduced)) assert.ok(!timing(p).fill, `${selector}: the reduced ${property} ${reduced} keeps a fill mode`);
            answered.push(selector);
        }
    }
}
// Named in the brief: the backdrop drift, the petals, the heartbeat, the pulse and the spin are each off.
for (const selector of ['.orb', '.petal', '.rays', '#roster .brand .art', '.heartwrap .heart', '.enroll .bond .art', '.glow', '.row .dot.on', '.step.here .medal',
    '.halo', '.spark', '.vial .sheen', '#popup .trophy > .art', '#popup .loss .trophy > .art', '#popup .bit']) {
    assert.equal(decls(REDUCED + selector).animation, 'none', selector + ' stops under reduced motion');
}
assert.equal(decls(REDUCED + '#petals').display, 'none', 'the petals go');
// Entrances become a short fade: the panes, the rows, the sheets.
for (const selector of ['#pane.enter > *', '#roster.enter .row', '#pane.enter .setrow', '.enroll', '#modal .sheet', '#popup .levelup']) {
    assert.match(decls(REDUCED + selector).animation || '', /^fade \.2s\b/, selector + ' enters by a short fade');
}
assert.ok(answered.length >= 30, 'the check saw the stylesheets\' animations: ' + answered.length);

console.log('Comfort (motion): ok');

/* ---------- stronger contrast: the tokens, measured ---------- */
const tokensText = read('css/tokens.css').replace(/\/\*[\s\S]*?\*\//g, '');
function block(selector) {
    const at = tokensText.indexOf(selector + ' {');
    assert.ok(at >= 0, 'tokens.css has a ' + selector + ' block');
    const open = tokensText.indexOf('{', at), close = tokensText.indexOf('}', open);
    const out = {};
    for (const d of tokensText.slice(open + 1, close).split(';')) { const k = d.indexOf(':'); if (k > 0) out[d.slice(0, k).trim()] = d.slice(k + 1).trim(); }
    return out;
}
const standard = block(':root'), high = block('html[data-contrast="high"]');
for (const token of ['--ink', '--mute', '--faint', '--glass-face', '--glass-edge', '--control-edge', '--row-edge', '--hairline', '--sheet-face', '--sheet-base',
    '--sheet-base-cool', '--focus-width', '--focus-ring', '--whisper-plate']) {
    assert.ok(token in standard, token + ' is a token');
    assert.ok(token in high, token + ' is redefined under stronger contrast');
    assert.notEqual(high[token], standard[token], token + ' changes under stronger contrast');
}
assert.equal(high['--hairline'], '2px', 'hairlines doubled');
assert.equal(standard['--hairline'], '1px');
assert.ok(parseFloat(high['--focus-width']) > parseFloat(standard['--focus-width']), 'a thicker focus ring');

// The components draw with the tokens, so the standard look is unchanged and the contrast one follows.
assert.equal(decls('.glass').background, 'var(--glass-face)');
assert.equal(decls('.glass').border, 'var(--hairline) solid var(--glass-edge)');
assert.equal(decls('.sheet').background, 'var(--sheet-face), var(--sheet-base)');
assert.equal(decls('.sheet').border, 'var(--hairline) solid var(--sheet-edge)');
assert.equal(decls('.row').border, 'var(--hairline) solid var(--row-edge)');
// The other edges thicken with the hairline, and the bright ones take their colour from a token.
assert.equal(decls('.chip').border, 'var(--hairline) solid rgba(255, 255, 255, .25)');
assert.equal(decls('.step .medal').border, 'var(--hairline) solid rgba(255, 255, 255, .22)');
assert.equal(decls('.enroll .cat')['border-bottom'], 'var(--hairline) solid rgba(255, 199, 217, .2)');
// The last literal 1px edges thicken with the hairline too; their colours, and so the standard look, are unchanged.
assert.equal(decls('.vial').border, 'var(--hairline) solid rgba(255, 255, 255, .28)');
assert.equal(decls('.setrow .jewel').border, 'var(--hairline) solid rgba(255, 255, 255, .22)');
assert.equal(decls('#notice .pill').border, 'var(--hairline) solid rgba(255, 255, 255, .34)');
assert.equal(decls('#legend .key').border, 'var(--hairline) solid rgba(255, 255, 255, .3)');
assert.equal(decls('.hitem')['border-top'], 'var(--hairline) solid rgba(255, 255, 255, .08)');
assert.equal(decls('.row:hover')['border-color'], 'var(--control-edge)');
assert.equal(decls('.row.selected')['border-color'], 'var(--row-edge-selected)');
assert.equal(decls('.btn.danger')['border-color'], 'var(--danger-edge)');
assert.equal(standard['--row-edge-selected'], 'rgba(255, 196, 214, .75)', 'the edge of the selected row is unchanged at standard');
assert.equal(standard['--danger-edge'], 'rgba(255, 120, 140, .7)', 'the danger edge is unchanged at standard');
assert.match(high['--row-edge-selected'] || '', /^#[0-9a-f]{6}$/i, 'and solid under stronger contrast');
assert.match(high['--danger-edge'] || '', /^#[0-9a-f]{6}$/i);
// The small tools (the bond tools, Journal) thicken and brighten with the rest.
assert.equal(decls('.tool').border, 'var(--hairline) solid var(--tool-edge)');
assert.equal(decls('.tool.danger')['border-color'], 'var(--tool-danger-edge)');
assert.equal(standard['--tool-edge'], 'rgba(255, 255, 255, .2)', 'the tool edge is unchanged at standard');
assert.equal(standard['--tool-danger-edge'], 'rgba(255, 120, 140, .5)');
assert.notEqual(high['--tool-edge'], standard['--tool-edge']);
assert.notEqual(high['--tool-danger-edge'], standard['--tool-danger-edge']);
// The switch's lit track covers its border at either hairline width.
for (const side of ['top', 'left', 'right', 'bottom']) assert.equal(decls('.switch::before')[side], 'calc(-1 * var(--hairline))', 'the lit track, ' + side);
for (const selector of ['html.keys :focus', 'html[data-ml-input="gamepad"] [data-ml-focused]']) {
    assert.match(decls(selector)['box-shadow'], /^0 0 0 var\(--focus-width\) var\(--focus-ring\),/, selector + ' rings with the tokens');
}
// Glass loses its sheen and glow, and the backdrop its petals and drifting orbs.
const HIGH = 'html[data-contrast="high"] ';
for (const selector of ['.glass::before', '.glass::after', '#petals', '.orb']) assert.equal(decls(HIGH + selector).display, 'none', selector + ' goes under stronger contrast');

// Colours: '#rrggbb', 'rgb(...)', 'rgba(...)'; a gradient of one colour reads as that colour.
function colour(value) {
    const one = /^linear-gradient\((.+),\s*\1\)$/.exec(value);
    if (one) value = one[1].trim();
    let m = /^#([0-9a-f]{6})$/i.exec(value);
    if (m) return [0, 2, 4].map(i => parseInt(m[1].slice(i, i + 2), 16)).concat(1);
    m = /^rgba?\(([^)]*)\)$/.exec(value);
    if (m) { const n = m[1].split(',').map(Number); return [n[0], n[1], n[2], n.length > 3 ? n[3] : 1]; }
    throw new Error('not a colour: ' + value);
}
const over = (fg, bg) => [0, 1, 2].map(i => fg[i] * fg[3] + bg[i] * (1 - fg[3])).concat(1);
const luminance = c => {
    const lin = v => { v /= 255; return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c[0]) + 0.7152 * lin(c[1]) + 0.0722 * lin(c[2]);
};
const ratio = (a, b) => { const [x, y] = [luminance(a), luminance(b)].sort((p, q) => q - p); return (x + 0.05) / (y + 0.05); };
export const measured = {};
for (const plate of ['--glass-face', '--sheet-base', '--sheet-base-cool', '--whisper-plate']) {
    const bg = colour(high[plate]);
    assert.equal(bg[3], 1, plate + ' is solid under stronger contrast');
    // A row's own wash sits on the glass plate: the brightest is a hovered row (16% white); measure on that too.
    for (const [name, base] of [[plate, bg], [plate + ' + row hover', over([255, 255, 255, 0.16], bg)]]) {
        const ink = ratio(over(colour(high['--ink']), base), base), mute = ratio(over(colour(high['--mute']), base), base);
        measured[name] = { ink: +ink.toFixed(2), mute: +mute.toFixed(2) };
        assert.ok(ink >= 7, `body text on ${name}: ${ink.toFixed(2)}:1 is under 7:1`);
        assert.ok(mute >= 4.5, `muted text on ${name}: ${mute.toFixed(2)}:1 is under 4.5:1`);
    }
}
if (process.env.COMFORT_RATIOS) console.log(JSON.stringify(measured, null, 1));

console.log('Comfort: ok');
