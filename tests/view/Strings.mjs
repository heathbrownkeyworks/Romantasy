// One file of words: every word the page shows lives in view/js/strings.js. t(key, values) looks a key up and fills
// its {places}; or(key, fallback) is for words the plugin sends (a tier name, a deed label), the table's text or the
// plugin's own. The words test below scans the page scripts and both HTML files for a visible word outside the table.
import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const VIEW = new URL('../../view/', import.meta.url);
const read = path => readFileSync(new URL(path, VIEW), 'utf8');
const STRINGS = read('js/strings.js');
const strings = require('../../view/js/strings.js');

/* ---------- t and or ---------- */
// A table of our own, through the same code: strings.js run in a box whose console we watch.
function boxed(source) {
    const logs = [];
    const box = { window: {}, console: { warn: (...a) => logs.push(a.join(' ')), log() {}, error() {} } };
    vm.runInNewContext(source, box);
    return { S: box.window.RM.strings, logs, box };
}
const { S, logs } = boxed(STRINGS);
assert.equal(typeof S.t, 'function');
assert.equal(typeof S.or, 'function');
assert.equal(typeof S.table, 'object');
S.table['test.place'] = '{name} needs {count} days ({name})';
assert.equal(S.t('test.place', { name: 'Lydia', count: 3 }), 'Lydia needs 3 days (Lydia)', 'every place is filled');
assert.equal(S.t('test.place', { name: 'Lydia' }), 'Lydia needs {count} days (Lydia)', 'an unknown place stays as written');
assert.equal(S.t('test.place'), '{name} needs {count} days ({name})', 'no values: the text as written');
assert.equal(S.t('test.place', null), '{name} needs {count} days ({name})');
assert.equal(S.t('test.place', { name: '$& $1 {count}', count: 0 }), '$& $1 {count} needs 0 days ($& $1 {count})', 'a value is inserted as it is, never read as a pattern or as a place');
// A table value that is not a string (a hand edit gone wrong) is filled in its string form, never thrown on.
S.table['test.number'] = 42;
assert.doesNotThrow(() => S.t('test.number', { a: 1 }));
assert.equal(S.t('test.number', { a: 1 }), '42');
// A missing key shows itself between brackets and is logged once, however often it is used.
// The first moment together, verbatim from the spec.
assert.equal(strings.t('journal.shared'), 'A first moment together');
assert.equal(S.t('no.such.key'), '[no.such.key]');
assert.equal(S.t('no.such.key', { a: 1 }), '[no.such.key]');
assert.equal(logs.filter(l => l.includes('no.such.key')).length, 1, 'one log for two uses');
assert.doesNotThrow(() => S.t(undefined));
assert.equal(S.t(undefined), '[undefined]');
// or: the table's text when it has the key, else the plugin's own text, never logged.
S.table['test.or'] = 'Fremder';
assert.equal(S.or('test.or', 'Stranger'), 'Fremder');
assert.equal(S.or('test.missing', 'Stranger'), 'Stranger');
assert.equal(logs.filter(l => l.includes('test.missing')).length, 0, 'a fallback is not a gap');
// Node gets the same API.
assert.equal(typeof strings.t, 'function');
assert.equal(typeof strings.or, 'function');

/* ---------- the table ---------- */
const table = strings.table;
const keys = Object.keys(table);
assert.ok(keys.length > 100, 'the table holds the page\'s words: ' + keys.length);
for (const key of keys) {
    assert.match(key, /^[a-z0-9]+(\.[a-z0-9]+)+$/, key + ': a key is dotted lower case, area first');
    assert.equal(typeof table[key], 'string', key + ' is text');
    assert.ok(table[key] !== '', key + ' is not empty');
    assert.ok(!/—/.test(table[key]), key + ' has no em dash');
}
// One line per key, so a translator changes only the text on the right.
for (const key of keys) {
    const lines = STRINGS.split('\n').filter(l => l.includes("'" + key + "':"));
    assert.equal(lines.length, 1, key + ' is on a line of its own');
}

/* ---------- the words test: no visible word outside the table ---------- */
// The literals of a script: comments skipped, regular expressions skipped, each string literal with where it starts.
function literals(source) {
    const out = [];
    const mask = source.split('');
    let i = 0, last = '';
    const blank = (a, b) => { for (let k = a; k < b; k++) if (mask[k] !== '\n') mask[k] = ' '; };
    while (i < source.length) {
        const c = source[i], n = source[i + 1];
        if (c === '/' && n === '/') { const e = source.indexOf('\n', i); const end = e < 0 ? source.length : e; blank(i, end); i = end; continue; }
        if (c === '/' && n === '*') { const end = source.indexOf('*/', i + 2) + 2; blank(i, end); i = end; continue; }
        if (c === '/' && (last === '' || /[(,=:[!&|?{};+\-*%<>~^]/.test(last))) {
            let k = i + 1, cls = false;
            while (k < source.length && (source[k] !== '/' || cls)) { if (source[k] === '\\') k++; else if (source[k] === '[') cls = true; else if (source[k] === ']') cls = false; k++; }
            blank(i, k + 1); i = k + 1; last = '/'; continue;
        }
        if (c === "'" || c === '"' || c === '`') {
            let k = i + 1, value = '';
            while (k < source.length && source[k] !== c) {
                if (source[k] === '\\') { value += source[k + 1] === 'n' ? '\n' : source[k + 1]; k += 2; continue; }
                value += source[k++];
            }
            out.push({ value, start: i, end: k + 1 });
            blank(i + 1, k); i = k + 1; last = 'x'; continue;
        }
        if (!/\s/.test(c)) last = c;
        i++;
    }
    return { list: out, masked: mask.join('') };
}

// Where a literal is never shown: inside a console call or an Error (messages for the log), or matched against data
// (compared with === or !==, or searched for with indexOf or includes).
function quietRanges(masked) {
    const ranges = [];
    for (const m of masked.matchAll(/console\.\w+\(|new Error\(/g)) {
        let depth = 0, k = m.index + m[0].length - 1;
        for (; k < masked.length; k++) { if (masked[k] === '(') depth++; else if (masked[k] === ')' && --depth === 0) break; }
        ranges.push([m.index, k]);
    }
    return ranges;
}
const matched = (masked, lit) => /(===|!==)\s*$|\.(indexOf|includes)\(\s*$/.test(masked.slice(Math.max(0, lit.start - 20), lit.start))
    || /^\s*(===|!==)/.test(masked.slice(lit.end, lit.end + 6));

// The exact literals a script may hold that look like words but are never shown, each with its reason.
const ALLOWED = new Set([
    // enrollment.js CATEGORY_ORDER and CATEGORY_ICON: the deed categories the plugin sends, matched to order them and pick a jewel.
    'Battle', 'Blood & Beast', 'Crime & Transgression', 'Exploration & Growth', 'Influence', 'Magic & Craft', 'Quests & Allegiances',
    // settings.js DEV: the statistics a developer button asks the plugin to count (message values, never shown).
    'Side Quests Completed', 'Murders',
    // tiers.js SENT: the tier names the plugin sends (RomanceManager's level names), matched to find a tier's level.
    'Stranger', 'Acquaintance', 'Friend', 'Confidant', 'Lover', 'Spouse',
    // pad.js keyName: the key's own name as the browser reports it, which pad.js returns to compare (the legend shows legend.key.enter).
    'Enter'
]);

// The class names the stylesheets know, and the one attribute the scripts add bare: a literal of these words
// (a class list such as 'art seek') is markup, not a sentence.
const CLASSES = new Set(['disabled']);
for (const file of readdirSync(new URL('css/', VIEW)).filter(f => f.endsWith('.css'))) {
    for (const m of read('css/' + file).replace(/\/\*[\s\S]*?\*\//g, '').matchAll(/\.([a-z][\w-]*)/gi)) CLASSES.add(m[1]);
}

// A literal shows a word when, past its markup, it holds a capitalised word or a word in capitals, a word that is
// not a class beside another word, or a lone word with a space on both sides that is not a class; or when an
// attribute the player sees (title, placeholder, aria-label, alt) holds a word. A single token (a key, a class, an id,
// an attribute name, an action or message name, a URL or a path) is never a word shown.
const LETTERS = /\p{L}{3,}/u;
function shownWord(text) {
    if (!LETTERS.test(text)) return false;
    for (const m of text.matchAll(/\b(title|placeholder|aria-label|alt)="([^"]*)"/g)) if (LETTERS.test(m[2])) return true;
    const rest = text.replace(/<\/?[a-z][^<>]*>/gi, ' ').replace(/^[^<>]*>/, ' ').replace(/<[a-z][^<>]*$/i, ' ')
        .replace(/[\w:-]+="[^"]*"/g, ' ').replace(/[\w:-]+="/g, ' ').replace(/&#?\w+;/g, ' ');
    if (!LETTERS.test(rest)) return false;
    const words = rest.split(/[^\p{L}\p{N}_\-.#:/[\]=()%]+/u).filter(w => LETTERS.test(w));
    if (words.some(w => /^\p{Lu}\p{Ll}+$/u.test(w) || /^\p{Lu}{3,}$/u.test(w))) return true;
    const plain = rest.split(/\s+/).map(w => w.replace(/^[("'.,:;!?·]+|[)"'.,:;!?·]+$/g, '')).filter(w => /^\p{Ll}{2,}$/u.test(w));
    if (plain.length > 1 && plain.some(w => !CLASSES.has(w))) return true;
    const lone = /^ +(\p{L}+)[ .,:;!?()]+$/u.exec(rest);
    if (lone && !CLASSES.has(lone[1])) return true;
    return false;
}
assert.ok(shownWord('Bond complete') && shownWord(' until ') && shownWord('placeholder="Find a companion"') && shownWord('Away')
    && shownWord(' days together (') && shownWord(' following</span>'));
assert.ok(!shownWord('<button class="btn round" data-action="refresh">') && !shownWord(' selected') && !shownWord('romantasySettings')
    && !shownWord('icons/') && !shownWord('[role="radio"]') && !shownWord('settings.dev-bond-up') && !shownWord('&amp;')
    && !shownWord('art seek') && !shownWord(' disabled aria-disabled="true"'));

const SCRIPTS = readdirSync(new URL('js/', VIEW)).filter(f => f.endsWith('.js') && f !== 'strings.js').sort();
const found = [];
const usedKeys = new Set(), usedPrefixes = new Set();
for (const file of SCRIPTS) {
    const source = read('js/' + file);
    const { list, masked } = literals(source);
    const quiet = quietRanges(masked);
    // The arguments of every t(, tx( (t escaped) and or( call: a key-shaped literal there is a key the table must hold.
    const calls = [];
    for (const m of masked.matchAll(/\b(t|tx|or)\(/g)) {
        let depth = 0, k = m.index + m[0].length - 1;
        for (; k < masked.length; k++) { if (masked[k] === '(') depth++; else if (masked[k] === ')' && --depth === 0) break; }
        calls.push([m.index, k]);
    }
    const KEY = /^[a-z0-9]+(\.[a-z0-9]+)+$/;
    for (const lit of list) {
        const inCall = calls.some(([a, b]) => lit.start > a && lit.start < b);
        if (/^[a-z0-9]+(\.[a-z0-9]+)*\.$/.test(lit.value) && inCall && /^\s*\+/.test(masked.slice(lit.end, lit.end + 4))) { usedPrefixes.add(lit.value); continue; }
        // A key held in a list for a later t() (an order's label, a mood's words) is used as well. No key has the text of
        // an action name (checked below), so an action literal never counts as a key's use.
        if (Object.prototype.hasOwnProperty.call(table, lit.value)) { usedKeys.add(lit.value); continue; }
        if (inCall && KEY.test(lit.value)) { usedKeys.add(lit.value); continue; }
        // 'use strict' is a directive, not a string the page shows.
        if (lit.value === 'use strict' || quiet.some(([a, b]) => lit.start > a && lit.start < b) || matched(masked, lit) || ALLOWED.has(lit.value)) continue;
        if (shownWord(lit.value)) found.push('js/' + file + ': ' + JSON.stringify(lit.value));
    }
}
// The HTML files: no text node shows a word (the title is the host's, never drawn; scripts are not text).
for (const file of ['index.html', 'whisper.html']) {
    const body = read(file).replace(/<!--[\s\S]*?-->/g, '').replace(/<script[\s\S]*?<\/script>/g, '').replace(/<title>[\s\S]*?<\/title>/g, '');
    for (const m of body.matchAll(/>([^<]+)</g)) if (LETTERS.test(m[1])) found.push(file + ': ' + JSON.stringify(m[1].trim()));
    for (const m of body.matchAll(/\b(title|placeholder|aria-label|alt)="([^"]*)"/g)) if (LETTERS.test(m[2])) found.push(file + ': ' + m[0]);
}
assert.deepEqual(found, [], 'words outside strings.js:\n  ' + found.join('\n  '));

// Every key a script names is in the table, and every key in the table is named by a script. A key built from a prefix
// (as 'tier.' + level) belongs to a family listed here by name; deed.<editor ID> lines are a translator's, never English.
const FAMILIES = { 'tier.': ['tier.1', 'tier.2', 'tier.3', 'tier.4', 'tier.5', 'tier.6'], 'deed.': [] };
assert.deepEqual([...usedPrefixes].filter(p => !FAMILIES[p]), [], 'every key prefix the scripts build is a listed family');
for (const p of Object.keys(FAMILIES)) {
    assert.deepEqual(keys.filter(k => k.startsWith(p)), FAMILIES[p], 'the table holds exactly the ' + p + ' family');
}
const missing = [...usedKeys].filter(k => !Object.prototype.hasOwnProperty.call(table, k));
assert.deepEqual(missing, [], 'keys the scripts use that the table lacks');
const familyKeys = [...usedPrefixes].reduce((all, p) => all.concat(FAMILIES[p]), []);
const unused = keys.filter(k => !usedKeys.has(k) && familyKeys.indexOf(k) < 0);
assert.deepEqual(unused, [], 'keys in the table that no script uses');
// No key has the text of an action (a data-action value or a name in a part's actions), so neither counts for the other.
const ACTIONS = new Set();
for (const file of SCRIPTS) {
    const source = read('js/' + file);
    for (const m of source.matchAll(/data-action="([\w.-]+)"/g)) ACTIONS.add(m[1]);
    for (const m of source.matchAll(/^\s*'([\w.-]+)':\s*(\(|el\b|\w+\s*=>)/gm)) ACTIONS.add(m[1]);
}
assert.ok(ACTIONS.has('roster.clear') && ACTIONS.has('journal.open'), 'the action names were found: ' + ACTIONS.size);
assert.deepEqual(keys.filter(k => ACTIONS.has(k)), [], 'keys that share their text with an action');

// strings.js loads before every script that uses it, in both pages.
for (const file of ['index.html', 'whisper.html']) {
    const scripts = [...read(file).matchAll(/<script\s+src="([^"]+)"/g)].map(m => m[1]);
    const at = scripts.indexOf('js/strings.js');
    assert.ok(at >= 0, file + ' loads strings.js');
    for (const src of scripts.slice(0, at)) assert.ok(!/\b(t|or)\(/.test(read(src).replace(/\/\/.*$/gm, '')), src + ' loads before strings.js but uses it');
}

/* ---------- review focus 5: a translated file that lacks a newer key, or has an extra one ---------- */
// The page drawn from a copy of strings.js with one line taken out and one unknown line added: the gap shows as
// [key], nothing throws, and the rest of the pane is drawn.
// The page's scripts in a box, as index.html loads them, after the given strings.js source; prepare(box) runs between
// the two (the render sweep below wraps t and or there). pad.js and whisper.js come too, for the legend and the card.
function pageWith(source, prepare) {
    const logs2 = [];
    const stub = () => ({ setAttribute() {}, getAttribute: () => null, removeAttribute() {}, classList: { add() {}, remove() {}, contains: () => false } });
    const box = { window: {}, console: { warn: (...a) => logs2.push(a.join(' ')), log() {}, error: (...a) => logs2.push('error ' + a.join(' ')) },
        document: { documentElement: stub(), getElementById: () => null, addEventListener() {} }, addEventListener() {} };
    vm.runInNewContext(source, box);
    if (prepare) prepare(box);
    for (const file of ['format.js', 'tiers.js', 'state.js', 'roster.js', 'ledger.js', 'settings.js', 'journal.js', 'popup.js', 'modals.js', 'enrollment.js', 'pad.js', 'whisper.js']) {
        vm.runInNewContext(read('js/' + file), box, { filename: file });
    }
    return { RM: box.window.RM, logs: logs2 };
}
const fixtureBox = { window: {} };
vm.runInNewContext(read('dev/fixture.js'), fixtureBox);
const lacking = STRINGS.split('\n').filter(l => !l.includes("'recent.title':")).join('\n')
    .replace("'values.title':", "'zz.unknown.key': 'Nobody asks for this',\n    'values.title':");
assert.notEqual(lacking, STRINGS);
const translated = pageWith(lacking);
const state = translated.RM.state.normalize(fixtureBox.window.ROMANTASY_FIXTURE);
let dossier = '';
assert.doesNotThrow(() => { dossier = translated.RM.ledger.dossierHtml(state.romances[0], state.preferenceOptions); });
assert.match(dossier, /<h3 class="eyebrow">\[recent\.title\]<\/h3>/, 'the gap shows as [key]');
assert.match(dossier, /What they value/, 'the rest of the pane is drawn');
assert.match(dossier, /Milestones/);
assert.doesNotThrow(() => translated.RM.ledger.rosterHtml(state, translated.RM.state.createView()));
assert.doesNotThrow(() => translated.RM.settings.paneHtml(state));
assert.equal(translated.logs.filter(l => l.includes('recent.title')).length, 1, 'the gap is logged once');
assert.ok(!dossier.includes('Nobody asks for this'), 'an extra key is harmless');

/* ---------- review focus 6: a value in a sentence is text, escaped once ---------- */
const ledger = require('../../view/js/ledger.js');
const model = require('../../view/js/state.js');
const evil = '<b>Tom & Jerry</b>';
const gated = model.normalize({ romances: [{ name: evil, role: evil, points: 900, levelName: 'Acquaintance', isFollowing: true,
    gate: { level: 3, name: evil, daysNeeded: 3, daysTogether: 1.5 }, profileFile: evil, profileOrigin: 'author', sourcePlugin: evil,
    referenceFormID: '0A', baseFormID: '0A' }] });
const html = ledger.dossierHtml(gated.romances[0], []);
assert.ok(html.includes('&lt;b&gt;Tom &amp; Jerry&lt;/b&gt; needs 3 days together (1.5 so far)'), 'the gate names its tier as text');
assert.ok(html.includes('Preferences loaded from &lt;b&gt;Tom &amp; Jerry&lt;/b&gt;.'), 'the file in the origin line is text');
assert.ok(!html.includes('<b>Tom'), 'no value reaches the page as markup');
assert.ok(!/&amp;(amp|lt|gt|quot|#39);/.test(html), 'nothing is escaped twice');
const journal = require('../../view/js/journal.js');
const j = journal.journalHtml({ name: evil, levelName: evil }, { identity: 'A', found: true, began: 2, daysTogether: 1,
    history: [{ label: evil, points: 5, day: 3 }] });
assert.ok(!j.includes('<b>Tom') && !/&amp;(amp|lt|gt);/.test(j), 'the journal escapes once');


/* ---------- the render sweep: every word the page draws comes from the table ---------- */
// A pseudo-locale copy of strings.js: each text becomes its key between ⟪ ⟫, then its {places} (so '{count} bonds' is
// '⟪roster.count.many⟫{count}'). Every pane, sheet, popup, legend and card is drawn from it, with t and or wrapped to
// record each key asked for. Then: (a) every key asked for is in the real table and nothing was logged as missing;
// (b) once the ⟪…⟫ runs and the data's own strings are taken out, no text or visible attribute holds three letters;
// (c) no [key] shows anywhere.
const PSEUDO = STRINGS.replace(/^(\s*)'([a-z0-9.]+)': (['"])(.*)\3(,?)$/gm,
    (line, indent, key, q, text, comma) => indent + "'" + key + "': '⟪" + key + '⟫' + (text.match(/\{\w+\}/g) || []).join('') + "'" + comma);
assert.notEqual(PSEUDO, STRINGS);
assert.ok(!/: '(?!⟪)[^']*'/.test(PSEUDO.slice(PSEUDO.indexOf('const table'), PSEUDO.indexOf('};'))), 'every line of the table was made pseudo');
const requested = new Set();
const sweep = pageWith(PSEUDO, box => {
    const S = box.window.RM.strings, t = S.t, or = S.or;
    S.t = (key, values) => { requested.add(key); return t(key, values); };
    S.or = (key, fallback) => { requested.add(key); return or(key, fallback); };
});
const P = sweep.RM;
const fx = JSON.parse(JSON.stringify(fixtureBox.window.ROMANTASY_FIXTURE));
// The data the sweep draws: the fixture and what is added here. Its strings are the plugin's words, never the page's.
const extra = {
    health: { status: 'warn', version: '3.1.0', host: '1.6.0', bonds: 1,
        items: [{ id: 'a', level: 'warn', title: 'Item warn', detail: 'Line one\nLine two' }, { id: 'b', level: 'error', title: 'Item error', detail: 'Detail' },
            { id: 'c', level: 'note', title: 'Item note', detail: 'Note detail' }] },
    unlocks: ['Unlock line one', 'Unlock line two'],
    popupGain: { followerName: 'Popup Gainer', changeDirection: 'gain', previousLevelName: 'Confidant', previousLevelIndex: 4, levelName: 'Lover', levelIndex: 5,
        nextLevelName: 'Spouse', points: 2010, pointsDelta: 25, unlocks: ['Popup unlock one', 'Popup unlock two'], seq: 1 },
    popupLoss: { followerName: 'Popup Loser', changeDirection: 'loss', previousLevelName: 'Friend', previousLevelIndex: 3, levelName: 'Acquaintance', levelIndex: 2,
        nextLevelName: 'Friend', points: 985, pointsDelta: -30, seq: 2 },
    journal: { identity: 'J', found: true, began: 2, daysTogether: 12.5,
        milestones: [{ kind: 'began', level: 1, day: 2 }, { kind: 'shared', level: 0, day: 4 }, { kind: 'reached', level: 3, day: 5 }, { kind: 'fell', level: 2, day: 9 }],
        days: [{ day: 4, gained: 30, lost: 0 }, { day: 5, gained: 10, lost: 20 }],
        gains: [{ label: 'Journal gain label', points: 30, day: 4 }], losses: [{ label: 'Journal loss label', points: -20, day: 5 }],
        history: [{ label: 'Journal history label', points: 10, day: 5 }] },
    whispers: [{ name: 'Whisper Name', line: 'Whisper line', tone: 'gain', icon: 'heart' }, { name: 'Whisper Two', line: 'Another line', tone: 'loss', icon: 'heartbroken' },
        { name: 'Whisper Three', line: 'Plain line', tone: 'plain', icon: 'nothing' }],
    gate: { level: 4, name: 'Confidant', daysNeeded: 3, daysTogether: 1.5 },
    gate1: { level: 3, name: 'Friend', daysNeeded: 1, daysTogether: 0.2 },
    find: 'zzzz no match'
};
const drawn = [];
const draw = (label, html) => drawn.push([label, String(html)]);
const st = P.state.normalize(Object.assign({}, fx, { health: extra.health, developerToolsUnlocked: true, skyrimNet: { found: true, bridge: true, conversation: true },
    ostim: { found: true, moments: true, hold: true } }));
for (const order of ['closest', 'name', 'recent']) {
    const s2 = Object.assign({}, st, { rosterOrder: order });
    draw('roster ' + order, P.ledger.rosterHtml(s2, P.state.createView()));
}
const noMatch = Object.assign(P.state.createView(), { find: extra.find });
draw('roster no match', P.ledger.rosterHtml(st, noMatch));
draw('empty no match', P.ledger.emptyHtml(st, noMatch));
draw('empty none', P.ledger.emptyHtml(P.state.normalize({}), P.state.createView()));
const awayOnly = P.state.normalize(Object.assign({}, fx, { showAwayFollowers: false, romances: fx.romances.map(r => Object.assign({}, r, { isFollowing: false })) }));
draw('empty away', P.ledger.emptyHtml(awayOnly, P.state.createView()));
const one = P.state.normalize(Object.assign({}, fx, { romances: fx.romances.slice(0, 1).map(r => Object.assign({}, r, { isFollowing: true })) }));
draw('roster one', P.ledger.rosterHtml(one, P.state.createView()));
const kinds = [{}, { mood: 'warm' }, { mood: 'settled' }, { mood: 'strained', isFollowing: false }, { gate: extra.gate }, { gate: extra.gate1 },
    { profileOrigin: 'author', profileFile: '', sourcePlugin: 'Plugin.esp', personalityEditable: false },
    { profileOrigin: 'author', profileFile: '', sourcePlugin: '', personalityEditable: false }, { profileOrigin: 'managed', profileFile: '', personalityEditable: false },
    { profileOrigin: 'player', personalityEditable: true, removable: true, recent: [] }, { points: 2600 }, { points: 0, role: '' }];
st.romances.forEach((row, i) => {
    for (const k of kinds) {
        const r = Object.assign({}, row, k);
        if (k.points !== undefined) r.levelName = P.tiers.tier(k.points).name;
        r.unlocks = [{ level: P.tiers.tierIndex(r.points) + 2, items: extra.unlocks }];
        r.gate = k.gate ? P.state.normalize({ romances: [{ gate: k.gate }] }).romances[0].gate : null;
        draw('dossier ' + i + ' ' + JSON.stringify(k), P.ledger.dossierHtml(r, st.preferenceOptions));
    }
});
draw('settings', P.settings.paneHtml(st));
for (const h of [{ status: 'ok', version: '3.0', host: '1.6', bonds: 1, items: [] }, { status: 'ok', version: '', host: '', bonds: 2, items: [] }]) {
    draw('health ' + h.bonds, P.settings.healthHtml(P.state.normalize({ health: h })));
}
const rowOne = st.romances[0];
for (const m of [{ kind: 'password', error: true }, { kind: 'password' }, { kind: 'reset', identity: 'x' }, { kind: 'remove', identity: 'x' }]) draw('modal ' + m.kind, P.modals.modalHtml(m, rowOne));
draw('popup gain', P.popup.popupHtml(extra.popupGain));
draw('popup loss', P.popup.popupHtml(extra.popupLoss));
for (const d of [null, { found: false }, { identity: 'J', found: true }, extra.journal, Object.assign({}, extra.journal, { began: 0, daysTogether: 1 })]) {
    draw('journal ' + JSON.stringify(d).slice(0, 30), P.journal.journalHtml(rowOne, d));
}
const opts = st.preferenceOptions;
const atCap = {};
opts.slice(0, 8).forEach((o, k) => { atCap[o.editorID] = [2, 2, 2, -2, -2, -2, 1, -1][k]; });
for (const t of [{ mode: 'create', identity: null, candidate: null, staged: {}, query: '' }, { mode: 'create', identity: null, candidate: null, staged: atCap, query: '' },
    { mode: 'create', identity: null, candidate: null, staged: {}, query: extra.find }, { mode: 'edit', identity: P.state.rowIdentity(rowOne), candidate: null, staged: atCap, query: '' },
    { mode: 'edit', identity: 'nobody', candidate: null, staged: {}, query: '' }]) {
    draw('takeover ' + t.mode + t.query, P.enrollment.takeoverHtml(st, t));
}
const loose = [{ editorID: 'ROM_Loose', label: 'Loose deed', points: 1 }];   // a deed with no category
draw('takeover uncategorised', P.enrollment.takeoverHtml(P.state.normalize({ preferenceOptions: loose }), { mode: 'create', identity: null, candidate: null, staged: {}, query: '' }));
draw('takeover bare', P.enrollment.takeoverHtml(P.state.normalize({}), { mode: 'create', identity: null, candidate: null, staged: {}, query: '' }));
for (const view of [{}, { pane: 'settings' }, { pane: 'journal' }, { takeover: {} }, { modal: { kind: 'password' } }, { modal: { kind: 'reset' } }, { modal: { kind: 'remove' } }, { popup: {} }]) {
    for (const device of ['keyboardMouse', 'gamepad']) {
        draw('legend ' + JSON.stringify(view) + device, P.pad.legend(view, device).map(x => '<b>' + x.prompt + '</b><span>' + x.label + '</span>').join(''));
    }
}
for (const w of extra.whispers) draw('whisper ' + w.tone, P.whisper.cardHtml(w));
draw('deeds', P.format.deedCount(1) + ' ' + P.format.deedCount(3));

// (a)
// A deed.<editor ID> key is the one kind or() may ask for without an English line (the plugin's label shows).
assert.deepEqual([...requested].filter(k => typeof k !== 'string' || (!Object.prototype.hasOwnProperty.call(table, k) && !/^deed\.\w+$/.test(k))), [],
    'keys the page asked for that the real table lacks');
assert.ok([...requested].some(k => /^deed\./.test(k)), 'the deed labels were asked for by key');
assert.deepEqual(sweep.logs, [], 'nothing was logged while drawing');
// (b) and (c)
const dataStrings = new Set();
(function collect(v) {
    if (typeof v === 'string') { for (const line of [v].concat(v.split(String.fromCharCode(10)))) if (/\p{L}{3,}/u.test(line)) dataStrings.add(line); }   // shorter ones could only hide letters
    else if (Array.isArray(v)) v.forEach(collect);
    else if (v && typeof v === 'object') Object.keys(v).forEach(k => collect(v[k]));
})([fx, extra, kinds, loose]);
const DATA = [...dataStrings].sort((a, b) => b.length - a.length);
const decode = s => s.replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&amp;/g, '&');
const leaks = [];
for (const [label, html] of drawn) {
    assert.ok(!/\[[a-z0-9]+(\.[a-z0-9]+)+\]/.test(html), label + ' shows a [key]');
    const parts = [html.replace(/<[^>]*>/g, ' ')];
    for (const m of html.matchAll(/\b(title|placeholder|aria-label|data-tip)="([^"]*)"/g)) parts.push(m[2]);
    for (const part of parts) {
        let text = decode(part).replace(/⟪[^⟫]*⟫/g, ' ');
        for (const d of DATA) text = text.split(d).join(' ');
        const m = /\p{L}{3,}/u.exec(text);
        if (m) leaks.push(label + ': "' + text.slice(Math.max(0, m.index - 30), m.index + 40).replace(/\s+/g, ' ').trim() + '"');
    }
}
assert.ok(drawn.length > 100, 'the sweep drew the page: ' + drawn.length);
assert.deepEqual([...new Set(leaks)], [], 'words drawn that are not from the table');
// Every key in the table was asked for by some drawing, except the gamepad's own defaults when the helper has names.
assert.deepEqual(keys.filter(k => !requested.has(k)), [], 'keys the sweep never drew');

/* ---------- a broken strings.js: the page still opens, closes and answers ---------- */
// No strings.js (it failed to load) and one that cannot be read (an apostrophe inside single quotes): format.js puts a
// stand-in in its place, logs one error naming strings.js, and every word shows as [key].
async function bootWith(source) {
    const errors = [];
    function element() {
        const attrs = {}, classes = new Set();
        return {
            innerHTML: '', className: '', textContent: '', value: '', style: {}, offsetWidth: 0, isConnected: true, disabled: false,
            classList: { add: c => classes.add(c), remove: c => classes.delete(c), toggle: (c, on) => (on === undefined ? !classes.has(c) : on) ? classes.add(c) : classes.delete(c), contains: c => classes.has(c) },
            setAttribute: (k, v) => { attrs[k] = String(v); }, getAttribute: k => (k in attrs ? attrs[k] : null), removeAttribute: k => { delete attrs[k]; },
            querySelector: () => null, querySelectorAll: () => [], addEventListener() {}, focus() {}, appendChild() {}, remove() {}, insertAdjacentHTML() {}
        };
    }
    const elements = new Map();
    const page = {
        console: { log() {}, warn() {}, error: (...a) => errors.push(a.join(' ')) }, setTimeout, clearTimeout, location: { search: '?mock' },
        requestAnimationFrame: fn => setTimeout(fn, 0),
        document: {
            readyState: 'complete', documentElement: element(), head: element(), activeElement: null,
            getElementById: id => { if (!elements.has(id)) elements.set(id, element()); return elements.get(id); },
            addEventListener() {}, createElement: element, querySelector: () => null, querySelectorAll: () => []
        },
        addEventListener() {}
    };
    page.window = page;
    page.RM = { fit: { apply() {} } };
    const run = (text, name) => vm.runInNewContext(text, page, { filename: name });
    run(read('js/bridge.js'), 'bridge.js');
    if (source !== null) { try { run(source, 'strings.js'); } catch (error) { /* the browser logs it and loads the next script */ } }
    for (const file of ['format.js', 'tiers.js', 'state.js', 'notice.js', 'enrollment.js', 'roster.js', 'ledger.js', 'settings.js', 'journal.js', 'modals.js', 'popup.js', 'app.js']) {
        run(read('js/' + file), file);
    }
    run(read('dev/fixture.js'), 'fixture.js');
    run(read('dev/mock.js'), 'mock.js');
    const RM = page.RM, log = page.__romantasyMock.log;
    assert.equal(RM.app.view.mode, 'ledger', 'the page opens');
    const roster = elements.get('roster').innerHTML;
    assert.match(roster, /<h1>\[brand\.title\]<\/h1>/, 'the roster is drawn, its words as [key]');
    assert.ok(RM.app.state.romances.every(r => roster.includes(r.name.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')) || !r.isFollowing), 'with its bonds');
    page.__romantasyMock.popup('gain');
    await new Promise(r => setTimeout(r, 400));
    assert.ok(log.some(e => e.name === 'romantasyPopupShown'), 'the popup says it was shown');
    RM.popup.dismiss();
    await new Promise(r => setTimeout(r, 400));
    assert.ok(log.some(e => e.name === 'romantasyPopupDismissed'), 'and that it went: ' + log.map(e => e.name).join(','));
    RM.app.dispatch('close', null);
    assert.ok(log.some(e => e.name === 'romantasyClose'), 'Close is sent');
    assert.equal(errors.filter(e => e.includes('strings.js')).length, 1, 'one error names strings.js');
    assert.equal(RM.strings.or('tier.1', 'Stranger'), 'Stranger', 'the stand-in gives the plugin its own words');
}
await bootWith(null);
const broken = STRINGS.replace("'settings.title': \"The scribe's quill\",", "'settings.title': 'The scribe's quill',");
assert.notEqual(broken, STRINGS, 'the broken copy differs');
await bootWith(broken);

console.log('Strings: ok');
