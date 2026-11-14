// The ledger is a game screen: nothing stretches with its content and the page never scrolls.
// Every card has explicit grid rows, clips itself, and gives its content one body that scrolls inside it
// with the ledger's slim scrollbar; no script scrolls anything but the nearest of those bodies.
import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const VIEW = new URL('../../view/', import.meta.url);
const read = path => readFileSync(new URL(path, VIEW), 'utf8');

/* ---------- the stylesheets, as rules ---------- */
// Top-level rules only (@keyframes and @font-face bodies are skipped); a selector list is split, so a
// declaration counts for every selector its rule names.
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
            for (const d of body.split(';')) {
                const k = d.indexOf(':');
                if (k > 0) decls[d.slice(0, k).trim()] = d.slice(k + 1).trim();
            }
            out.push({ file, selectors: head.split(',').map(s => s.trim().replace(/\s+/g, ' ')), decls });
        }
        i = j;
    }
    return out;
}
const FILES = readdirSync(new URL('css/', VIEW)).filter(f => f.endsWith('.css'));
// The rules below are the ledger's: only the stylesheets index.html loads (whisper.css is another page,
// checked by Whisper.mjs). The older-WebKit check further down still covers every file.
const LEDGER_FILES = FILES.filter(f => read('index.html').includes('href="css/' + f + '"'));
const ALL = LEDGER_FILES.flatMap(rules);
// Every declaration for one selector, in source order (later wins).
const decls = selector => Object.assign({}, ...ALL.filter(r => r.selectors.includes(selector)).map(r => r.decls));

// The page never scrolls: the document and the stage clip.
assert.equal(decls('html').overflow, 'hidden', 'html clips');
assert.equal(decls('body').overflow, 'hidden', 'body clips');
assert.equal(decls('#app').overflow, 'hidden', '#app clips');

// Grids that hold cards have explicit rows, so a renderer cannot size a row to its content.
assert.equal(decls('.mid')['grid-template-rows'], 'minmax(0, 1fr)', '.mid has an explicit row');
assert.equal(decls('.values')['grid-template-rows'], 'minmax(0, 1fr)', '.values has an explicit row');
assert.match(decls('#pane.dossier')['grid-template-rows'] || '', /^280px minmax\(0, 1fr\) 212px$/, 'the dossier keeps hero 280, milestones 212');
// The smaller stages give the dossier other fixed rows, but its middle track may still shrink below its content.
for (const size of ['large', 'largest']) {
    assert.match(decls('html[data-size="' + size + '"] #pane.dossier')['grid-template-rows'] || '', /^\d+px minmax\(0, 1fr\) \d+px$/,
        'at ' + size + ' the dossier middle track is minmax(0, 1fr) between fixed rows');
}
// The milestone steps share the strip evenly, whatever a tier name's width; a name keeps to its step on one line.
assert.equal(decls('.steps')['grid-template-columns'], 'repeat(6, minmax(0, 1fr))', 'a long tier name never widens its step');
for (const sel of ['.step .tname', '.values h4']) {
    assert.equal(decls(sel)['white-space'], 'nowrap', sel + ' keeps to one line');
    assert.equal(decls(sel).overflow, 'hidden', sel + ' never spills past its column');
    assert.equal(decls(sel)['text-overflow'], 'ellipsis', sel + ' ends in an ellipsis when it is longer than its column');
}
assert.match(decls('#app')['grid-template-rows'] || '', /minmax\(0, 1fr\)/);
// Every card clips and may shrink below its content.
assert.equal(decls('.card')['min-height'], '0', '.card may shrink');
assert.equal(decls('.card').overflow, 'hidden', '.card clips');

// Each card's one scrolling body.
const BODIES = ['#roster .rows', '.list', '.recent', '.persona-card .body', '.miles .unlocks', '.setlist', '.jbody',
    '.enroll .cands', '.enroll .deeds', '.enroll .chips', '#popup .unlocks ul'];
for (const body of BODIES) {
    const d = decls(body);
    assert.equal(d['overflow-y'], 'auto', body + ' scrolls inside itself');
    assert.equal(d['min-height'], '0', body + ' may shrink below its content');
}
// The roster head keeps its height; the rows list is the roster's one body and scrolls under it.
assert.equal(decls('#roster .rhead').flex, 'none', 'the roster head never shrinks or grows');
assert.ok(!('overflow-y' in decls('#roster .rhead')), 'the head does not scroll');
assert.equal(decls('#roster .rows').flex, '1', 'the rows list takes the rest of the roster');
for (const sel of ['#roster .group', '#roster .rows .none']) assert.equal(decls(sel).flex, 'none', sel + ' keeps its height in the list');
assert.ok(!/nowrap/.test(decls('.miles .unlocks')['white-space'] || ''), 'the unlock lines wrap instead of being cut');
// The counts under "Your Company" stay inside the roster however many bonds: the heading keeps its width, the counts
// may shrink below their one line and then break between their two parts (each part never breaks), and two lines of
// them stay under the heading's own line, so the head keeps its height.
const counts = decls('#roster .counts');
assert.equal(counts['min-width'], '0', 'the counts may shrink below their one line');
assert.ok(!/nowrap/.test(counts['white-space'] || ''), 'and break between their parts');
assert.equal(decls('#roster .counts > span')['white-space'], 'nowrap', 'each part keeps to one line');
assert.equal(decls('#roster .counts > span')['max-width'], '100%', 'a part is never wider than the counts');
assert.equal(decls('#roster .counts > span')['text-overflow'], 'ellipsis', 'a longer part ends in an ellipsis');
// "Your Company" keeps its width while it fits; a longer translation shrinks it, and it ends in an ellipsis.
const company = decls('#roster .company em');
assert.equal(company.flex, 'none', 'the heading keeps its width; the counts give way first');
assert.equal(company['max-width'], '60%', 'up to 60% of the line, so the counts keep room');
assert.equal(company['white-space'], 'nowrap');
assert.equal(company.overflow, 'hidden');
assert.equal(company['text-overflow'], 'ellipsis', 'a longer heading ends in an ellipsis');
// The order choice: three equal places; a word too long for its place ends in an ellipsis, never pushing another out.
const orderOpt = decls('#roster .order .opt');
assert.equal(orderOpt.flex, '1');
assert.equal(orderOpt['min-width'], '0', 'an order word may shrink below its width');
assert.equal(orderOpt['white-space'], 'nowrap');
assert.equal(orderOpt.overflow, 'hidden');
assert.equal(orderOpt['text-overflow'], 'ellipsis', 'a long order word ends in an ellipsis');
// The hero name: one line, an ellipsis when it is longer than its column.
for (const prop of [['white-space', 'nowrap'], ['overflow', 'hidden'], ['text-overflow', 'ellipsis']]) {
    assert.equal(decls('.hero .name')[prop[0]], prop[1], 'the hero name: ' + prop.join(' '));
}
// A Settings choice row: the options may move under the title and wrap among themselves; the row grows in height only
// (the Settings list scrolls), never past its own edge.
const choiceRow = decls('.setrow.choice');
assert.equal(choiceRow['flex-wrap'], 'wrap', 'the options move under the title when both cannot share the line');
assert.equal(choiceRow.height, 'auto');
assert.equal(decls('.setrow.choice .text').flex, '1 1 40%', 'the title keeps at least 40% of the line beside the options');
const choices = decls('.choices');
assert.equal(choices['flex-wrap'], 'wrap', 'the options wrap among themselves');
assert.equal(choices['max-width'], '100%', 'and never run past the row');
assert.equal(choices.flex, '0 1 auto');
// At Largest the tier popup's trophy is smaller, so a two-line Unlocked list shows both lines.
assert.equal(decls('html[data-size="largest"] #popup .trophy').height, '160px', 'a smaller trophy at Largest');
assert.equal(decls('html[data-size="largest"] #popup .trophy > .art').height, '144px');
for (const size of ['large', 'standard']) assert.equal(decls('html[data-size="' + size + '"] #popup .trophy').height, undefined, 'unchanged at ' + size);
const lineOf = font => { const m = /(\d+(?:\.\d+)?)px\/(\d*\.?\d+)/.exec(font); return Number(m[1]) * Number(m[2]); };
assert.ok(2 * lineOf(counts.font) <= lineOf(decls('#roster .company em').font), 'two lines of counts fit under the line of the heading');
// The New bond sheet fits the smallest stage (1280 wide at the Largest size): never wider than the stage less its margins.
assert.equal(decls('.enroll')['max-width'], 'calc(100% - 56px)', 'the takeover sheet fits a narrower stage');
assert.ok(!('max-height' in decls('#popup .unlocks ul')), 'the popup list grows with the sheet');
assert.equal(decls('#popup .levelup')['max-height'], 'calc(100% - 80px)', 'the popup grows up to the stage less 80');

// The ledger's scrollbar: slim, a rose-gold thumb from the tokens, a clear track; never hidden.
const bar = decls('.scroller::-webkit-scrollbar');
assert.equal(bar.width, '6px', 'a 6px scrollbar');
assert.match(decls('.scroller::-webkit-scrollbar-thumb').background || '', /^var\(--[\w-]+\)$/, 'the thumb is a token');
assert.match(decls('.scroller::-webkit-scrollbar-thumb')['border-radius'] || '', /px/, 'the thumb is rounded');
assert.equal(decls('.scroller::-webkit-scrollbar-track').background, 'transparent', 'the track is clear');
const tokens = read('css/tokens.css');
const thumb = decls('.scroller::-webkit-scrollbar-thumb').background.slice(4, -1);
assert.match(tokens, new RegExp(thumb + ':\\s*rgba\\(244, 198, 173, \\.\\d+\\)'), 'the thumb token is the rose gold at low alpha');
for (const r of ALL) {
    if (r.selectors.some(s => /::-webkit-scrollbar$/.test(s)) && r.decls.width === '0') assert.fail(r.file + ': ' + r.selectors.join(', ') + ' hides its scrollbar');
}

// The dossier eyeline holds the role and the chips: the chips never shrink, the role is the one thing that gives way,
// and no fixed reserve stands in for anything. No rule places the bond tools in the hero.
assert.equal(decls('.chip').flex, 'none', 'the chips never shrink');
assert.equal(decls('.hero .eyeline .eyebrow')['text-overflow'], 'ellipsis', 'the role gives way with an ellipsis');
for (const r of ALL) assert.ok(!r.selectors.some(s => /\.hero\b.*\.tools?\b/.test(s)), r.selectors.join(', ') + ' places tools in the hero');
// The Milestones top is one flex row of the heading's own line height, so it cannot grow the card: the heading gives way
// with an ellipsis before it runs under what follows it, and the head beside it (the unlock lines, or those and the bond
// tools) has one fixed height and never wraps; the tools row and each tool may shrink, a tool's label ends in an ellipsis.
const mtop = decls('.miles .mtop');
assert.equal(mtop.display, 'flex');
assert.equal(mtop['flex-wrap'], 'nowrap', 'heading and head share one line');
assert.equal(mtop.height, '11px', 'the top row is as high as the heading line');
assert.ok(!Object.values(decls('.miles .mhead')).concat(Object.values(decls('.miles .unlocks'))).some(v => /^absolute$/.test(v)), 'no fixed place: the row lays them out');
const mheading = decls('.miles .mtop > h3.eyebrow');
assert.equal(mheading.flex, '0 1 auto', 'the heading may shrink');
assert.equal(mheading['min-width'], '0');
assert.equal(mheading['max-width'], '40%', 'the heading leaves the head most of the row');
const htext = decls('.miles .mtop > h3.eyebrow > .htext');
assert.equal(htext['white-space'], 'nowrap');
assert.equal(htext.overflow, 'hidden');
assert.equal(htext['text-overflow'], 'ellipsis', 'a long heading ends in an ellipsis');
const mhead = decls('.miles .mhead');
assert.match(mhead.height || '', /^\d+px$/, 'the head row has a fixed height');
assert.match(decls('.miles .unlocks').height || '', /^\d+px$/, 'the unlock lines have a fixed height');
assert.equal(mhead['flex-wrap'], 'nowrap', 'the head row never wraps');
assert.equal(mhead['justify-content'], 'flex-end', 'the tools are right-aligned');
const toolRow = decls('.miles .tools');
assert.equal(toolRow['flex-wrap'], 'nowrap');
assert.equal(toolRow['min-width'], '0', 'the tools row may shrink');
const oneTool = decls('.miles .tools .tool');
assert.equal(oneTool['min-width'], '0', 'a tool may shrink');
assert.equal(oneTool['white-space'], 'nowrap');
assert.equal(oneTool.overflow, 'hidden');
assert.equal(oneTool['text-overflow'], 'ellipsis', 'a label too long ends in an ellipsis');
assert.ok(parseInt(decls('.tool').height, 10) <= parseInt(mhead.height, 10), 'a tool fits the head row\'s height');
for (const r of ALL) {
    if (r.selectors.some(s => /eyeline/.test(s))) assert.ok(!('padding-right' in r.decls), r.selectors.join(', ') + ' reserves a fixed width');
}
// The Strained chip's words are lighter than its dim edge and dot: a token of their own.
assert.equal(decls('.chip.mood.strained').color, 'var(--mood-strained-text)');
assert.match(tokens, /--mood-strained-text:\s*#[0-9a-f]{6};/i);

// A deed label of more than two lines ends in an ellipsis.
const label = decls('.deed .label');
assert.equal(label['-webkit-line-clamp'], '2');
assert.equal(label.display, '-webkit-box');
assert.equal(label['-webkit-box-orient'], 'vertical');

// What an older WebKit cannot do.
for (const file of FILES) {
    const text = read('css/' + file).replace(/\/\*[\s\S]*?\*\//g, '');
    for (const pattern of [/overflow(-[xy])?\s*:\s*clip/, /scrollbar-gutter/, /@container/, /:has\(/, /(^|[\s;{])inset\s*:/, /aspect-ratio/]) {
        assert.ok(!pattern.test(text), file + ' uses ' + pattern);
    }
}

/* ---------- no script scrolls the page ---------- */
for (const file of readdirSync(new URL('js/', VIEW)).filter(f => f.endsWith('.js'))) {
    assert.ok(!/scrollIntoView/.test(read('js/' + file)), 'js/' + file + ' calls scrollIntoView, which can scroll #app or the document');
}

/* ---------- the bodies in the markup ---------- */
const s = require('../../view/js/state.js');
const ledger = require('../../view/js/ledger.js');
const settings = require('../../view/js/settings.js');
const enrollment = require('../../view/js/enrollment.js');
const popup = require('../../view/js/popup.js');
const journal = require('../../view/js/journal.js');
const box = { window: {} };
vm.runInNewContext(read('dev/fixture.js'), box);
const state = s.normalize(box.window.ROMANTASY_FIXTURE);
const view = { ...s.createView(), selected: s.rowIdentity(state.romances[0]) };
const row = { ...state.romances[0], unlocks: [{ level: 6, items: ['A wedding at the Temple of Mara'] }] };

// A body with no focus stop of its own is one (tabindex 0), so keys and the pad can scroll it.
const FOCUSABLE_BODY = /class="([\w -]*\bscroller\b[\w -]*)" tabindex="0"/g;
const bodiesIn = html => [...html.matchAll(FOCUSABLE_BODY)].map(m => m[1].split(' ')[0]);
assert.deepEqual(bodiesIn(ledger.dossierHtml(row, state.preferenceOptions)), ['list', 'list', 'recent', 'body', 'unlocks'],
    'the value lists, Recent changes, the Personality text and the unlock lines are focusable bodies');
assert.match(ledger.rosterHtml(state, view), /<div class="rows scroller">/, 'the roster rows scroll (their rows are the focus stops)');
assert.match(settings.paneHtml(state), /<div class="setlist scroller">/, 'the settings list scrolls');
// With SkyrimNet and OStim both found at the Largest size (?mock&ostim&skyrimnet&size=largest), every row, the four
// extra ones included, still sits inside that one list: more rows make the list scroll, never the page.
{
    const full = settings.paneHtml({ ...state, ledgerSize: 'largest', skyrimNet: { found: true, bridge: true, conversation: true },
        ostim: { found: true, moments: true, hold: true } });
    const listAt = full.indexOf('<div class="setlist scroller">');
    assert.ok(listAt > 0 && full.endsWith('</div>'), 'the rows close inside the list');
    for (const key of ['skyrimNetBridge', 'skyrimNetConversation', 'ostimMoments', 'ostimHold', 'ledgerSize']) {
        assert.ok(full.indexOf('data-key="' + key + '"') > listAt, key + ' is a row of the scrolling list');
    }
    assert.equal(full.split('class="setrow').length - 1, 17, 'seventeen rows, all in the list');
    assert.equal(full.split('scroller').length - 1, 1, 'and the list is the pane\'s one scrolling body');
}
const t = { mode: 'create', identity: null, candidate: null, staged: {}, query: '' };
const sheet = enrollment.takeoverHtml(state, t);
assert.match(sheet, /class="cands scroll scroller"/);
assert.match(sheet, /class="deeds scroll scroller"/);
assert.deepEqual(bodiesIn(sheet), ['chips', 'chips'], 'the two summary columns are focusable bodies');
const gain = popup.popupHtml({ followerName: 'A', changeDirection: 'gain', levelName: 'Lover', unlocks: ['A key', 'A ring'] });
assert.match(gain, /<ul class="scroller" tabindex="0">/, 'the popup\'s Unlocked list is a focusable body');
// The Journal pane: a fixed head over one body that scrolls, whatever the answer holds; the chart's bars are
// sized inside a box of fixed height, never by their content.
const fullest = {
    identity: 'A', found: true, began: 1, daysTogether: 99.5,
    milestones: Array.from({ length: 26 }, (_, i) => ({ kind: i ? 'fell' : 'began', level: 1 + (i % 6), day: i + 1 })),
    days: Array.from({ length: 60 }, (_, i) => ({ day: i + 1, gained: i * 3, lost: 60 - i })),
    gains: Array.from({ length: 5 }, (_, i) => ({ label: 'g'.repeat(120), points: 50 - i, day: i + 1 })),
    losses: Array.from({ length: 5 }, (_, i) => ({ label: 'l'.repeat(120), points: i - 50, day: i + 1 })),
    history: Array.from({ length: 30 }, (_, i) => ({ label: 'h '.repeat(60), points: i - 15, day: 60 - i }))
};
for (const data of [null, { found: false }, fullest]) {
    assert.deepEqual(bodiesIn(journal.journalHtml(row, data)), ['jbody'], 'the Journal pane has one focusable body');
}
assert.equal(decls('#pane.journal').display, 'flex');
assert.equal(decls('#pane.journal')['flex-direction'], 'column');
assert.equal(decls('.jhead').flex, 'none', 'the head keeps its height');
assert.match(decls('.jhead').height || '', /^\d+px$/, 'a fixed head');
assert.equal(decls('.jbody').flex, '1', 'the body takes the rest');
assert.match(decls('.jbars').height || '', /^\d+px$/, 'the chart has a fixed height');
for (const sel of ['.jday .gain', '.jday .loss']) assert.equal(decls(sel).position, 'absolute', sel + ' is sized by its inline height, not its content');
// A day's numbers on hover, drawn by the page: out of the flow (no shift), on one line, never catching the pointer. The day
// itself is not positioned, so the tip sits on the chart box (centred over the axis), not on the bar at either edge.
const tip = decls('.jday:hover::after');
assert.equal(tip.content, 'attr(data-tip)', 'the tip shows the day\'s data-tip');
assert.equal(tip.position, 'absolute', 'the tip takes no room');
assert.equal(tip['white-space'], 'nowrap');
assert.equal(tip['pointer-events'], 'none');
assert.equal(tip.left, '50%', 'centred on the chart');
assert.match(tip.transform || '', /^translateX\(-50%\)$/);
assert.ok(!('position' in decls('.jday')), 'the day is not the tip\'s anchor');
assert.equal(decls('.jbars').position, 'relative', 'the chart is');
assert.equal(decls('.jday .jbar').position, 'relative', 'the bar halves anchor to the bar');
for (const [k, v] of Object.entries(tip)) if (/color|background|^border$/.test(k)) assert.match(v, /var\(--[\w-]+\)/, 'the tip\'s ' + k + ' is a token');
for (const sel of ['.jhead .jname', '.jfacts .jtier', '.jfacts .since']) {
    assert.equal(decls(sel)['text-overflow'], 'ellipsis', sel + ' gives way with an ellipsis');
    assert.equal(decls(sel)['white-space'], 'nowrap', sel + ' keeps to one line');
}
for (const sel of ['.jlist .label', '.jmile']) assert.equal(decls(sel)['overflow-wrap'], 'break-word', sel + ' breaks a long word');

// Each focusable body has a stable data-ml-id, so focus finds it again after a repaint.
for (const html of [ledger.dossierHtml(row, state.preferenceOptions), sheet, journal.journalHtml(row, fullest)]) {
    for (const m of html.matchAll(/<[a-z]+ class="[^"]*\bscroller\b[^"]*" tabindex="0"([^>]*)>/g)) assert.match(m[1], /data-ml-id="[\w-]+"/, 'a body has a data-ml-id');
}

// The deed label carries its full text as a title.
assert.match(sheet, /<span class="label" title="[^"]+">/);

console.log('NoStretch: ok');
