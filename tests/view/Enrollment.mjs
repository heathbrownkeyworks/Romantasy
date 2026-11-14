import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const e = require('../../view/js/enrollment.js');
const s = require('../../view/js/state.js');
const f = require('../../view/js/format.js');
const sandbox = { window: {} };
vm.runInNewContext(readFileSync(new URL('../../view/dev/fixture.js', import.meta.url), 'utf8'), sandbox);
const fixture = sandbox.window.ROMANTASY_FIXTURE;
const state = s.normalize(fixture);

const opts = fixture.preferenceOptions;
assert.equal(opts.length, 58);
const groups = e.groupByCategory(opts);
assert.deepEqual(groups.map(g => g.category), e.CATEGORY_ORDER);
assert.equal(groups.reduce((n, g) => n + g.options.length, 0), 58);
const loc = opts.find(o => o.editorID === 'ROM_LocationsDiscovered');
assert.ok(e.matches(loc, 'locat')); assert.ok(e.matches(loc, 'EXPLORATION')); assert.ok(e.matches(loc, '  '));
assert.ok(!e.matches(loc, 'murder'));
const page = e.buildPage(opts, { ROM_LocationsDiscovered: 1 });
assert.equal(page.length, 58);
assert.deepEqual(page.filter(p => p[1] !== 0), [['ROM_LocationsDiscovered', 1]]);
assert.deepEqual(e.stagedFromRow({ likes: ['Locations Discovered', 'No Such Deed'], dislikes: ['Murders'] }, opts),
    { ROM_LocationsDiscovered: 1, [opts.find(o => o.label === 'Murders').editorID]: -1 });
// The key cycle: neutral, like, favorite, dislike, pet peeve, neutral; a capped step is skipped.
assert.deepEqual([0, 1, 2, -1, -2].map(d => e.cycle(d)), [1, 2, -1, -2, 0]);
assert.equal(e.cycle(1, { favorite: true }), -1, 'favorites full: like goes to dislike');
assert.equal(e.cycle(-1, { peeve: true }), 0, 'pet peeves full: dislike goes to neutral');
assert.equal(e.cycle(1, { peeve: true }), 2);
assert.equal(e.cycle(2, { favorite: true, peeve: true }), -1);
assert.equal(e.candidateMeta({ role: 'Imperial · Mage', sourcePlugin: 'Skyrim.esm' }), 'Imperial · Mage · Skyrim.esm');
assert.equal(e.candidateMeta({ role: '', sourcePlugin: '' }), 'Companion');
assert.equal(e.candidateMeta({ role: 'Mage', sourcePlugin: '' }), 'Mage');
assert.equal(e.candidateMeta({ role: '', sourcePlugin: 'A.esp' }), 'A.esp');
assert.equal(e.canCommit({ mode: 'create', candidate: null, staged: { X: 1 } }), false);
assert.equal(e.canCommit({ mode: 'create', candidate: '0001A6A6', staged: {} }), false);
assert.equal(e.canCommit({ mode: 'create', candidate: '0001A6A6', staged: { X: 1 } }), true);
assert.equal(e.canCommit({ mode: 'edit', candidate: null, staged: { X: -1 } }), true);
assert.equal(e.canCommit({ mode: 'edit', candidate: null, staged: { X: 0 } }), false);
assert.deepEqual(e.groupByCategory([]), []); assert.deepEqual(e.buildPage([], {}), []);

// Unknown categories sort after the known ones, alphabetically; only non-empty groups appear.
const odd = [{ editorID: 'a', label: 'A', category: 'Zeta' }, { editorID: 'b', label: 'B', category: 'Alpha' }, { editorID: 'c', label: 'C', category: 'Battle' }];
assert.deepEqual(e.groupByCategory(odd).map(g => g.category), ['Battle', 'Alpha', 'Zeta']);
const sum = e.summary(opts, { ROM_LocationsDiscovered: 1, [opts.find(o => o.label === 'Murders').editorID]: -1 });
assert.deepEqual(sum, { favorites: [], likes: ['Locations Discovered'], dislikes: ['Murders'], petPeeves: [] });

/* ---------- the takeover's markup ---------- */
const t = { mode: 'create', identity: null, candidate: null, staged: {}, query: '' };
const html = e.takeoverHtml(state, t);
assert.equal((html.match(/data-candidate=/g) || []).length, 2);
assert.equal((html.match(/data-deed=/g) || []).length, 58);
for (const c of e.CATEGORY_ORDER) assert.ok(html.includes(f.esc(c)));
assert.match(html, /Begin bond/); assert.match(html, /disabled/);
const filtered = e.takeoverHtml(state, { ...t, query: 'murder' });
assert.ok((filtered.match(/data-deed=/g) || []).length < 58);
assert.match(e.takeoverHtml(state, { ...t, query: '<zz>' }), /No deeds match &quot;&lt;zz&gt;&quot;\./);
assert.match(e.takeoverHtml(state, { ...t, mode: 'edit', identity: 'X', staged: { ROM_LocationsDiscovered: 1 } }), /Seal personality/);
assert.match(e.takeoverHtml(s.normalize({}), t), /No companions are available/);

// Every deed carries the five stamps, the staged one pressed; the commit button frees up once committable.
const stamps = h => [...h.matchAll(/<button class="stamp [^"]*"[^>]*data-direction="(-?\d)"[^>]*aria-pressed="(true|false)"/g)].map(m => [Number(m[1]), m[2] === 'true']);
assert.equal(stamps(html).length, 58 * 5);
assert.deepEqual(stamps(html).filter(x => x[1]).map(x => x[0]), new Array(58).fill(0), 'every deed starts neutral');
const murders = opts.find(o => o.label === 'Murders').editorID;
const ready = { ...t, candidate: '0001A6A6', staged: { ROM_LocationsDiscovered: 1, [murders]: -1 } };
const readyHtml = e.takeoverHtml(state, ready);
assert.deepEqual(stamps(readyHtml).filter(x => x[1] && x[0] !== 0).map(x => x[0]).sort(), [-1, 1]);
assert.doesNotMatch(readyHtml, /data-action="enroll\.commit" disabled/);
assert.match(readyHtml, /class="row cand selected" data-action="enroll\.pick" data-candidate="0001A6A6"/);
assert.match(e.barHtml(state, ready), /Deepens the bond<b class="count">1<\/b>[\s\S]*Locations Discovered[\s\S]*Wounds the bond<b class="count">1<\/b>[\s\S]*Murders/);
assert.equal((e.barHtml(state, t).match(/Nothing stamped yet\./g) || []).length, 2);
assert.match(e.takeoverHtml(state, { ...t, mode: 'edit', identity: 'FE964801' }), /Seal personality[^<]*<\/button>/);
assert.match(e.takeoverHtml(state, { ...t, mode: 'edit', identity: 'FE964801' }), /Inalion/);

// Empty cases: no candidates keeps commit disabled; no deeds at all.
const bare = s.normalize({});
assert.match(e.takeoverHtml(bare, { ...t, staged: { ROM_LocationsDiscovered: 1 } }), /data-action="enroll\.commit" disabled/);
assert.match(e.takeoverHtml(bare, t), /No deeds are available\./);
assert.match(e.takeoverHtml({ ...state, preferenceOptions: [] }, t), /No deeds are available\./);

// Markup, quotes and other alphabets are escaped everywhere, attributes included.
const hostile = '"><img src=x onerror=1>\'';
const nasty = {
    ...state,
    enrollmentCandidates: [{ name: 'Серана <b>', role: hostile, sourcePlugin: '', referenceFormID: hostile, baseFormID: '' }],
    preferenceOptions: [{ editorID: hostile, label: '<i>Deed</i> "q"', category: '<Cat>' }],
    romances: state.romances.map((r, i) => i === 2 ? { ...r, name: 'Серана <b>' + hostile, role: hostile } : r)
};
for (const h of [e.takeoverHtml(nasty, t), e.takeoverHtml(nasty, { ...t, mode: 'edit', identity: 'FE964801', staged: { [hostile]: 1 } })]) {
    assert.doesNotMatch(h, /<img src=x|<b>|<i>|<Cat>/);
    assert.ok(!h.includes(hostile), 'nothing hostile survives unescaped');
    for (const attr of h.matchAll(/\s[\w-]+="([^"]*)"/g)) assert.ok(!/[<>]/.test(attr[1]), 'attribute values carry no raw markup');
}
assert.match(e.takeoverHtml(nasty, t), /Серана &lt;b&gt;/);
assert.match(e.takeoverHtml(nasty, { ...t, mode: 'edit', identity: 'FE964801' }), /Серана &lt;b&gt;/);

// reconcile: the edited bond gone closes; the chosen candidate gone drops the choice.
assert.equal(e.reconcile(state, { ...t, mode: 'edit', identity: 'NOPE' }), null);
const kept = { ...t, mode: 'edit', identity: 'FE964801' };
assert.equal(e.reconcile(state, kept), kept);
assert.equal(e.reconcile(state, ready), ready);
assert.equal(e.reconcile({ ...state, enrollmentCandidates: [] }, ready).candidate, null);
assert.deepEqual(e.reconcile({ ...state, enrollmentCandidates: [] }, ready).staged, ready.staged);
assert.equal(e.reconcile(state, null), null);

/* ---------- favorites and pet peeves ---------- */
const id = label => opts.find(o => o.label === label).editorID;
const DUN = id('Dungeons Cleared'), QST = id('Quests Completed'), MUR = id('Murders'), STO = id('Items Stolen');
assert.deepEqual(e.stagedFromRow({ likes: ['Dungeons Cleared', 'Quests Completed'], favorites: ['Dungeons Cleared'],
    dislikes: ['Murders', 'Items Stolen'], petPeeves: ['Murders'] }, opts), { [DUN]: 2, [QST]: 1, [MUR]: -2, [STO]: -1 });
const strong = e.buildPage(opts, { [DUN]: 2, [QST]: 1, [MUR]: -2, [STO]: -1 });
assert.deepEqual(strong.filter(p => p[1] !== 0).sort(), [[DUN, 2], [QST, 1], [MUR, -2], [STO, -1]].sort(), 'the page sends -2 to 2');
assert.deepEqual(e.summary(opts, { [DUN]: 2, [QST]: 1, [MUR]: -2, [STO]: -1 }),
    { favorites: ['Dungeons Cleared'], likes: ['Quests Completed'], dislikes: ['Items Stolen'], petPeeves: ['Murders'] });
// The five stamps, in order, with their directions.
const firstDeed = html.slice(html.indexOf('<div class="deed'), html.indexOf('</div>', html.indexOf('<div class="deed')));
assert.deepEqual([...firstDeed.matchAll(/data-direction="(-?\d)"[^>]*title="([^"]*)"/g)].map(m => [Number(m[1]), m[2]]),
    [[2, 'Favorite'], [1, 'Deepens the bond'], [0, 'Neutral'], [-1, 'Wounds the bond'], [-2, 'Pet peeve']]);
assert.match(firstDeed, /class="stamp like"/); assert.match(firstDeed, /class="stamp neutral on"/); assert.match(firstDeed, /class="stamp dislike"/);
// The cap: three favorites staged disables the Favorite stamp on every other deed, not on the three.
const stampTag = (h, deed, d) => (h.match(new RegExp('<button class="stamp[^"]*" data-action="enroll\.stamp" data-of="' + deed + '" data-direction="' + d + '"[^>]*>')) || [''])[0];
const favs = { [DUN]: 2, [QST]: 2, ROM_LocationsDiscovered: 2 };
const capped = e.takeoverHtml(state, { ...t, staged: favs });
for (const k of Object.keys(favs)) assert.doesNotMatch(stampTag(capped, k, 2), /disabled/, k + ' keeps its Favorite stamp');
assert.match(stampTag(capped, MUR, 2), / disabled aria-disabled="true"/, 'a fourth deed cannot be a favorite');
assert.match(stampTag(capped, MUR, 2), /title="Three favorites at most\."/);
assert.doesNotMatch(stampTag(capped, MUR, -2), /disabled/);
assert.ok(capped.includes('Three favorites at most.'), 'the cap line shows');
assert.ok(!capped.includes('Three pet peeves at most.'));
assert.ok(!e.takeoverHtml(state, { ...t, staged: { [DUN]: 2, [QST]: 2 } }).includes('Three favorites at most.'), 'no cap line below three');
const peeves = { [MUR]: -2, [STO]: -2, ROM_LocationsDiscovered: -2 };
const cappedP = e.takeoverHtml(state, { ...t, staged: peeves });
for (const k of Object.keys(peeves)) assert.doesNotMatch(stampTag(cappedP, k, -2), /disabled/, k + ' keeps its Pet peeve stamp');
assert.match(stampTag(cappedP, DUN, -2), / disabled aria-disabled="true"/);
assert.match(stampTag(cappedP, DUN, -2), /title="Three pet peeves at most\."/);
assert.doesNotMatch(stampTag(cappedP, DUN, 2), /disabled/);
assert.ok(cappedP.includes('Three pet peeves at most.'));
// The cap line itself, beside its list in the summary bar (the disabled stamps' titles carry the same words):
// at three and not at two, for favorites and for pet peeves.
const capLine = words => '<span class="cap">' + words + '</span>';
assert.ok(e.barHtml(state, { ...t, staged: favs }).includes(capLine('Three favorites at most.')), 'the bar shows the favorites cap line at three');
assert.ok(!e.barHtml(state, { ...t, staged: { [DUN]: 2, [QST]: 2 } }).includes('<span class="cap">'), 'and no cap line at two');
assert.ok(e.barHtml(state, { ...t, staged: peeves }).includes(capLine('Three pet peeves at most.')), 'the bar shows the pet peeves cap line at three');
assert.ok(!e.barHtml(state, { ...t, staged: { [MUR]: -2, [STO]: -2 } }).includes('<span class="cap">'), 'and no cap line at two');
// The summary bar carries the tags.
const bar = e.barHtml(state, { ...t, staged: { [DUN]: 2, [QST]: 1, [MUR]: -2 } });
assert.match(bar, /Deepens the bond<b class="count">2<\/b>/); assert.match(bar, /Wounds the bond<b class="count">1<\/b>/);
assert.match(bar, /Dungeons Cleared[\s\S]*>Favorite<[\s\S]*Quests Completed/);
assert.match(bar, /Murders[\s\S]*>Pet peeve</);
// A row with four favorites (never sent by the plugin) stages only three, so the page never sends four.
const four = e.stagedFromRow({ likes: ['Dungeons Cleared', 'Quests Completed', 'Murders', 'Items Stolen'],
    favorites: ['Dungeons Cleared', 'Quests Completed', 'Murders', 'Items Stolen'] }, opts);
assert.equal(Object.values(four).filter(v => v === 2).length, 3);
assert.equal(e.buildPage(opts, { [DUN]: 2, [QST]: 2, [MUR]: 2, [STO]: 2 }).filter(p => p[1] === 2).length, 3);

console.log('Enrollment: ok');
