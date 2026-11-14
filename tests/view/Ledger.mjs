import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const s = require('../../view/js/state.js');
const l = require('../../view/js/ledger.js');

const sandbox = { window: {} };
vm.runInNewContext(readFileSync(new URL('../../view/dev/fixture.js', import.meta.url), 'utf8'), sandbox);
const state = s.normalize(sandbox.window.ROMANTASY_FIXTURE);
const view = s.createView();
view.selected = s.rowIdentity(state.romances[0]);

const html = l.rosterHtml(state, view);
assert.equal((html.match(/data-identity=/g) || []).length, 8);
assert.match(html.replace(/<[^>]*>/g, ''), /8 bonds · \d following/);
assert.match(html, /<span class="counts"><span>8 bonds<\/span> <span>· \d following<\/span><\/span>/, 'the counts break only between their two parts');
assert.match(html, /2,240 \/ 2,500/);
assert.match(html, /Серана &lt;b&gt;/); assert.doesNotMatch(html, /Серана <b>/);
const hidden = l.rosterHtml({ ...state, showAwayFollowers: false }, view);
assert.ok((hidden.match(/data-identity=/g) || []).length < 8);
const player = state.romances.find(r => r.removable);
const d = l.dossierHtml(player);
for (const a of ['edit', 'reset', 'remove']) assert.match(d, new RegExp(`data-action="${a}"`));
const authored = state.romances.find(r => !r.removable && !r.personalityEditable);
assert.doesNotMatch(l.dossierHtml(authored), /data-action="(edit|reset|remove)"/);
assert.match(l.dossierHtml({ ...authored, likes: [], dislikes: [], recent: [] }), /No likes recorded\./);
assert.match(l.dossierHtml({ ...authored, points: 2630 }), /Bond complete/);
assert.match(l.dossierHtml({ ...authored, name: '"><img src=x>' }), /&quot;&gt;&lt;img/);
assert.match(l.emptyHtml(s.normalize({})), /No bonds yet\./);
assert.match(l.emptyHtml(s.normalize({ showAwayFollowers: false, romances: [{ name: 'A', isFollowing: false }] })), /Every bond is away\./);

// The selected row is marked; the roster carries its four controls.
assert.match(html, new RegExp(`class="row selected"[^>]*data-identity="${view.selected}"`));
for (const a of ['new-bond', 'settings', 'refresh', 'close']) assert.match(html, new RegExp(`data-action="${a}"`));
// Empty lists and recent changes read as the brief words them; the Personality card drops empty lines.
const bare = l.dossierHtml({ ...authored, likes: [], dislikes: [], recent: [], sourcePlugin: '', profileFile: '' });
assert.match(bare, /No dislikes recorded\./); assert.match(bare, /No recent changes\./);
assert.doesNotMatch(bare, /Source plugin|Profile file/);
assert.match(l.dossierHtml(authored), /Author-defined personality/);
assert.match(l.dossierHtml(player), /Player-created personality/);
// The Personality card words the origin in four cases, in order.
const seal = extra => l.dossierHtml({ ...authored, sourcePlugin: '', profileFile: '', personalityEditable: false, ...extra });
const configured = seal({ profileOrigin: 'author', profileFile: 'Aelira.toml' });
assert.match(configured, /Configured personality/); assert.match(configured, /Preferences loaded from Aelira\.toml\./);
assert.doesNotMatch(configured, /Author-defined/);
assert.match(seal({ profileOrigin: 'player', personalityEditable: true }), /Player-created personality/);
assert.match(seal({ profileOrigin: 'player', personalityEditable: true, profileFile: 'x.toml' }), /Player-created personality/);
const sealed = seal({ profileOrigin: 'author', sourcePlugin: 'CSV_Aelira.esp' });
assert.match(sealed, /Author-defined personality/); assert.match(sealed, /in CSV_Aelira\.esp/);
assert.match(seal({ profileOrigin: 'author' }), /Sealed by the follower author\. Romantasy will not alter or remove it\./);
assert.match(seal({ profileOrigin: '' }), /Managed personality/);
assert.match(seal({ profileOrigin: 'player', personalityEditable: false }), /Managed personality/);
assert.match(seal({ profileOrigin: 'author', profileFile: '<x>.toml' }), /Preferences loaded from &lt;x&gt;\.toml\./);

// Every state string is escaped, wherever it lands.
const hostile = l.dossierHtml({ ...player, role: '<i>', likes: ['<u>'], dislikes: ['<s>'], recent: [{ label: '<q>', points: 3, when: '<p>' }],
    sourcePlugin: '<e>.esp', profileFile: '<f>.toml' });
assert.doesNotMatch(hostile, /<(i|u|s|q|p|e|f)>/);

// One row whose every string carries markup and quotes, through the roster (as the selected row) and the dossier.
const X = '"><img src=x onerror=1>\'';
const evil = s.normalize({ romances: [{
    name: 'N' + X, role: 'R' + X, levelName: 'L' + X, sourcePlugin: 'P' + X, profileFile: 'F' + X, profileOrigin: 'author',
    referenceFormID: 'I' + X, baseFormID: 'B' + X, points: 1240, isFollowing: true, personalityEditable: true, removable: true,
    likes: ['K' + X], dislikes: ['D' + X], recent: [{ label: 'A' + X, points: 4, when: 'W' + X }]
}] }).romances[0];
const evilView = { ...s.createView(), selected: s.rowIdentity(evil) };
const OPEN_TAG = /<[a-zA-Z][\w-]*(?:\s+[\w:-]+(?:="[^"<>']*")?)*\s*\/?>/y;
const CLOSE_TAG = /<\/[a-zA-Z][\w-]*>/y;
function wellFormed(out, where) {
    assert.doesNotMatch(out, /<img src=x|onerror=1>/, `${where}: hostile markup reached the page unescaped`);
    for (let i = out.indexOf('<'); i >= 0; i = out.indexOf('<', i + 1)) {
        OPEN_TAG.lastIndex = i; CLOSE_TAG.lastIndex = i;
        assert.ok(OPEN_TAG.test(out) || CLOSE_TAG.test(out), `${where}: malformed tag or attribute at ${JSON.stringify(out.slice(i, i + 80))}`);
    }
}
const ESCAPED = '&quot;&gt;&lt;img src=x onerror=1&gt;&#39;';
const evilRoster = l.rosterHtml({ ...s.normalize({}), romances: [evil] }, evilView);
wellFormed(evilRoster, 'roster');
assert.match(evilRoster, /class="row selected"/);
for (const p of ['N', 'R', 'L']) assert.ok(evilRoster.includes('>' + p + ESCAPED), `roster shows ${p} escaped`);
assert.ok(evilRoster.includes('data-identity="I' + ESCAPED + '"'));
const evilDossier = l.dossierHtml(evil);
wellFormed(evilDossier, 'dossier');
for (const p of ['N', 'R', 'L', 'P', 'F', 'K', 'D', 'A', 'W']) assert.ok(evilDossier.includes('>' + p + ESCAPED), `dossier shows ${p} escaped`);
assert.ok(evilDossier.includes('loaded from F' + ESCAPED + '.'), 'the configured line escapes the profile file');
const evilAuthor = l.dossierHtml({ ...evil, profileFile: '' });
wellFormed(evilAuthor, 'author dossier');
assert.ok(evilAuthor.includes('in P' + ESCAPED + '. Romantasy'), 'the author line escapes the plugin');
for (const a of ['edit', 'reset', 'remove']) assert.ok(evilDossier.includes(`data-action="${a}" data-target="I${ESCAPED}"`), `${a} target escaped`);
assert.ok(evilDossier.includes(`data-action="journal.open" data-target="I${ESCAPED}"`), 'the Journal target escaped');

/* ---------- the Journal button: every dossier has one, at the foot of Recent changes ---------- */
for (const r of [player, authored]) {
    const out = l.dossierHtml(r);
    const button = `<button class="tool journal-open" data-action="journal.open" data-target="${s.rowIdentity(r)}" data-ml-id="journal-open">Journal</button>`;
    assert.equal(out.split(button).length, 2, 'one Journal button for ' + r.name);
    const card = out.slice(out.indexOf('<section class="card glass recent-card">'), out.indexOf('</section>', out.indexOf('recent-card')));
    assert.ok(card.endsWith('</ul>' + button), 'after the Recent changes list, in its card');
}

/* ---------- favorites, pet peeves, what one event is worth, the next tier's unlock lines ---------- */
const OPTIONS = [
    { editorID: 'ROM_Quests', label: 'Quests Completed', points: 3, category: 'Quests & Allegiances' },
    { editorID: 'ROM_Dungeons', label: 'Dungeons Cleared', points: 2, category: 'Exploration & Growth' },
    { editorID: 'ROM_Murders', label: 'Murders', points: 5, category: 'Crime & Transgression' },
    { editorID: 'ROM_Stolen', label: 'Items Stolen', points: 1, category: 'Crime & Transgression' }
];
const friend = s.normalize({ romances: [{ ...authored, points: 1200, levelName: 'Friend',
    likes: ['Quests Completed', 'Dungeons Cleared'], favorites: ['Dungeons Cleared'],
    dislikes: ['Items Stolen', 'Murders'], petPeeves: ['Murders'],
    unlocks: [{ level: 2, items: ['Old line'] }, { level: 4, items: ['A key to her house', 'Her mother\'s ring'] }, { level: 5, items: ['Later'] }] }] }).romances[0];
const rich = l.dossierHtml(friend, OPTIONS);
const column = (out, cls) => out.slice(out.indexOf('<h4 class="' + cls + '"'), out.indexOf('</ul>', out.indexOf('<h4 class="' + cls + '"')));
const items = out => [...out.matchAll(/<li class="item[^"]*">([\s\S]*?)<\/li>/g)].map(m => m[1]);
const deepens = items(column(rich, 'deepens'));
assert.equal(deepens.length, 2);
assert.match(deepens[0], /Dungeons Cleared/); assert.match(deepens[0], />Favorite</); assert.match(deepens[0], />\+4</); assert.match(deepens[0], /icons\/star\.png/);
assert.match(deepens[1], /Quests Completed/); assert.match(deepens[1], />\+3</); assert.doesNotMatch(deepens[1], /Favorite/);
const wounds = items(column(rich, 'wound'));
assert.equal(wounds.length, 2);
assert.match(wounds[0], /Murders/); assert.match(wounds[0], />Pet peeve</); assert.match(wounds[0], />−10</); assert.match(wounds[0], /icons\/heartbroken\.png/);
assert.match(wounds[1], /Items Stolen/); assert.match(wounds[1], />−1</); assert.doesNotMatch(wounds[1], /Pet peeve/);
// A label with no matching option shows its row without a value.
assert.doesNotMatch(items(column(l.dossierHtml(friend, []), 'deepens'))[0], /\+\d/);
assert.ok(rich.replace(/<[^>]+>/g, '').includes('Confidant unlocks: A key to her house · Her mother&#39;s ring'), 'the next tier names its unlock lines');
assert.doesNotMatch(rich, /Old line|Later/);
assert.doesNotMatch(l.dossierHtml({ ...friend, unlocks: [{ level: 5, items: ['Later'] }] }, OPTIONS), /unlocks:/, 'no line when the next tier has none');
assert.doesNotMatch(l.dossierHtml({ ...friend, points: 2630, unlocks: [{ level: 6, items: ['Ring'] }] }, OPTIONS), /unlocks:/, 'no line at Spouse');
const escapedUnlock = l.dossierHtml({ ...friend, unlocks: [{ level: 4, items: ['<b>x</b> & y'] }] }, OPTIONS);
assert.ok(escapedUnlock.includes('&lt;b&gt;x&lt;/b&gt; &amp; y'), 'an unlock line is escaped');
wellFormed(escapedUnlock, 'unlock dossier');
const plainRow = l.dossierHtml(authored, OPTIONS);
assert.doesNotMatch(plainRow, />Favorite<|>Pet peeve<|unlocks:/, 'a row with none of them shows no tag and no unlock line');
assert.doesNotMatch(l.dossierHtml(authored), />Favorite<|unlocks:/, 'and without options too');

/* ---------- mood: a second chip on the eyeline, after Following (or Away) ---------- */
const eyeline = out => { const at = out.indexOf('<div class="eyeline">'); return out.slice(at, out.indexOf('</div>', at)); };
const MOODS = [['warm', 'Warm', 'Warm: recent days went well.'], ['settled', 'Settled', 'Settled: nothing recent weighs on the bond.'],
    ['strained', 'Strained', 'Strained: recent days hurt the bond.']];
for (const [mood, word, title] of MOODS) {
    const chip = '<span class="chip mood ' + mood + '" title="' + title + '">' + word + '</span>';
    const following = eyeline(l.dossierHtml({ ...authored, isFollowing: true, mood }));
    assert.ok(following.endsWith('<span class="chip on">Following</span>' + chip), `${mood}: the chip follows the Following chip`);
    const away = eyeline(l.dossierHtml({ ...authored, isFollowing: false, mood }));
    assert.ok(away.endsWith('<span class="chip">Away</span>' + chip), `${mood}: an away bond shows it in the same place`);
    wellFormed(l.dossierHtml({ ...authored, mood }), mood + ' dossier');
}
for (const row of [{ ...authored, mood: '' }, s.normalize({ romances: [{ ...authored, mood: 'warm" onclick="x' }] }).romances[0]]) {
    const line = eyeline(l.dossierHtml(row));
    assert.equal((line.match(/class="chip/g) || []).length, 1, 'no mood, no second chip');
    assert.doesNotMatch(line, /mood|onclick/);
}

// The eyeline holds the role and the chips; the bond tools sit in the head of the Milestones card, after its heading,
// in their order, each with its label as a title; the hero holds no copy of them.
const toolsRow = l.dossierHtml({ ...player, isFollowing: true, mood: 'warm' });
const line = toolsRow.slice(toolsRow.indexOf('<div class="eyeline">'), toolsRow.indexOf('<h2'));
const lineOrder = ['<span class="eyebrow">', '<span class="chip on">Following</span>', '<span class="chip mood warm"'].map(part => line.indexOf(part));
assert.ok(lineOrder.every((at, i) => at >= 0 && (i === 0 || at > lineOrder[i - 1])), 'eyebrow, Following, then mood: ' + lineOrder);
const heroOf = out => out.slice(0, out.indexOf('</section>') + 10);
assert.doesNotMatch(heroOf(toolsRow), /class="tools?\b|data-action="(edit|reset|remove)"/, 'the hero holds no tool');
const milesOf = out => out.slice(out.indexOf('<section class="miles glass">'));
const head = milesOf(toolsRow);
const toolOrder = ['<div class="mtop"><h3 class="eyebrow" title="Milestones"><span class="htext">Milestones</span></h3>', '<div class="mhead">', '<div class="tools">',
    '<button class="tool" data-action="edit" data-target="' + s.rowIdentity(player) + '" title="Edit personality">Edit personality</button>',
    '<button class="tool" data-action="reset" data-target="' + s.rowIdentity(player) + '" title="Reset bond">Reset bond</button>',
    '<button class="tool danger" data-action="remove" data-target="' + s.rowIdentity(player) + '" title="Remove bond">Remove bond</button>',
    '<div class="steps">'].map(part => head.indexOf(part));
assert.ok(toolOrder.every((at, i) => at >= 0 && (i === 0 || at > toolOrder[i - 1])), 'Milestones, then the head row, then the three tools, then the steps: ' + toolOrder);
assert.equal((toolsRow.match(/class="tools"/g) || []).length, 1, 'one tools row');
// Text that may end in an ellipsis carries its whole text as a title: the hero name, the Milestones heading, Your Company.
const longName = 'A'.repeat(43) + ' <i>';
assert.ok(l.dossierHtml({ ...player, name: longName }).includes('<h2 class="name longer" title="' + 'A'.repeat(43) + ' &lt;i&gt;">'), 'the hero name has its title');
assert.ok(l.rosterHtml(state, view).includes('<em class="foil" title="Your Company">Your Company</em>'), 'Your Company has its title');
assert.ok(toolsRow.indexOf('<section class="hero glass"><div class="heartwrap">') === 0, 'the hero opens on the heart, as before');
// Which tools a bond gets is unchanged: Edit with an editable personality, Reset and Remove when removable; none, no row.
const authoredOut = l.dossierHtml(authored);
assert.doesNotMatch(authoredOut, /class="mhead"|class="tools"|class="tool( danger)?" data-action/, 'a bond with no tools has the Milestones head it had');
assert.doesNotMatch(l.dossierHtml({ ...player, removable: false }), /data-action="(reset|remove)"/);
assert.match(l.dossierHtml({ ...player, removable: false }), /data-action="edit"/);
assert.doesNotMatch(l.dossierHtml({ ...player, personalityEditable: false }), /data-action="edit"/);

/* ---------- the Slow burn gate: a line under the meter while a gate is ahead ---------- */
const meterOf = out => { const at = out.indexOf('<div class="meter">'); return out.slice(at, out.indexOf('</section>', at)); };
const gated = s.normalize({ romances: [{ ...authored, points: 900, gate: { level: 3, name: 'Friend', daysNeeded: 3, daysTogether: 1.46 } }] }).romances[0];
const gatedHtml = l.dossierHtml(gated);
assert.ok(meterOf(gatedHtml).includes('>Friend needs 3 days together (1.4 so far)<'), 'the gate line sits with the meter');
assert.equal((gatedHtml.match(/needs \d+ days together/g) || []).length, 1, 'once');
wellFormed(gatedHtml, 'gated dossier');
assert.doesNotMatch(l.dossierHtml({ ...gated, gate: null }), /days together/, 'no gate, no line');
assert.doesNotMatch(l.dossierHtml(authored), /days together/, 'a row from the plain fixture has none');
const gateLine = gate => /needs [^<]*/.exec(l.dossierHtml({ ...gated, gate: { ...gated.gate, ...gate } }))[0];
assert.equal(gateLine({ daysTogether: 1 }), 'needs 3 days together (1.0 so far)', 'one decimal place, always');
assert.equal(gateLine({ daysTogether: 0.04 }), 'needs 3 days together (0.0 so far)');
assert.equal(gateLine({ daysNeeded: 21, daysTogether: 20.96 }), 'needs 21 days together (20.9 so far)', 'never rounded up to the needed number');
assert.equal(gateLine({ daysTogether: 2.96 }), 'needs 3 days together (2.9 so far)');
assert.equal(gateLine({ daysNeeded: 14, daysTogether: 9.7 }), 'needs 14 days together (9.7 so far)', 'a tenth survives binary arithmetic');
// The tier name comes from data: it goes in as text.
const evilGate = l.dossierHtml({ ...gated, gate: { ...gated.gate, name: '<img src=x onerror=1>' } });
assert.ok(evilGate.includes('&lt;img src=x onerror=1&gt; needs 3 days together'));
wellFormed(evilGate, 'hostile gate');

/* ---------- the roster head: find, order; the groups ---------- */
{
    const head = html.slice(html.indexOf('<div class="rhead">'), html.indexOf('<div class="rows scroller">'));
    assert.ok(head.length > 0, 'a head above the rows');
    assert.match(head, /<input class="findbox" type="text"[^>]* placeholder="Find a companion"/, 'the find field');
    assert.doesNotMatch(head, /<input[^>]* value=/, 'the find text is never written into the markup');
    assert.match(head, /<button class="clear"[^>]* data-action="roster\.clear"[^>]* disabled>/, 'a clear button, off while there is nothing to clear');
    const group = /<div class="order" role="radiogroup" aria-label="Order">([\s\S]*?)<\/div>/.exec(head);
    assert.ok(group, 'the order choice is a radiogroup');
    const options = [...group[1].matchAll(/<button class="opt" role="radio" aria-checked="(true|false)" data-action="roster\.order" data-value="(\w+)" title="(\w+)">(\w+)<\/button>/g)]
        .map(m => [m[2], m[4], m[1], m[3]]);
    // Each option carries its word as a title too: a word too long for its place ends in an ellipsis there.
    assert.deepEqual(options, [['closest', 'Closest', 'true', 'Closest'], ['name', 'Name', 'false', 'Name'], ['recent', 'Recent', 'false', 'Recent']]);
    assert.match(l.rosterHtml({ ...state, rosterOrder: 'recent' }, view), /aria-checked="true" data-action="roster\.order" data-value="recent"/);
    assert.match(l.rosterHtml(state, { ...view, find: 'a' }), /<button class="clear"[^>]* data-action="roster\.clear"[^>]*>/);
    assert.doesNotMatch(l.rosterHtml(state, { ...view, find: 'a' }), /data-action="roster\.clear"[^>]* disabled/, 'on once there is text');

    const following = state.romances.filter(r => r.isFollowing).length, away = state.romances.length - following;
    const headings = [...html.matchAll(/<h3 class="group" data-group="(\w+)"><span class="gname">(\w+)<\/span><span class="gcount">(\d+)<\/span><\/h3>/g)].map(m => [m[1], m[2], Number(m[3])]);
    assert.deepEqual(headings, [['following', 'Following', following], ['away', 'Away', away]], 'a heading with its count for each group');
    assert.ok(html.indexOf('data-group="away"') > html.lastIndexOf('<button class="row', html.indexOf('data-group="away"')), 'Following rows come before the Away heading');
    assert.deepEqual([...hidden.matchAll(/data-group="(\w+)"/g)].map(m => m[1]), ['following'], 'no Away heading while away companions are hidden');
    // Closest by default: the rows run by points inside each group.
    const pointsOf = out => [...out.matchAll(/<span class="pts">([\d,]+)/g)].map(m => Number(m[1].replace(/,/g, '')));
    const firstGroup = pointsOf(html).slice(0, following);
    assert.deepEqual(firstGroup, firstGroup.slice().sort((a, b) => b - a));

    // Find: only the matching rows; the counts follow.
    const found = l.rosterHtml(state, { ...view, find: 'AELI' });
    assert.equal((found.match(/data-identity=/g) || []).length, 1);
    assert.match(found, /<span class="gname">Following<\/span><span class="gcount">1<\/span>/);
    assert.doesNotMatch(found, /data-group="away"/);
    const nobody = l.rosterHtml(state, { ...view, find: 'zzz' });
    assert.match(nobody, /<p class="none">No one by that name\.<\/p>/);
    assert.equal((nobody.match(/data-identity=|data-group=/g) || []).length, 0);
    assert.doesNotMatch(html, /No one by that name/, 'not while rows show');
    assert.doesNotMatch(l.rosterHtml(s.normalize({}), s.createView()), /No one by that name/, 'nor with no bonds at all');
    // Find text with markup in it reaches no markup.
    wellFormed(l.rosterHtml(state, { ...view, find: '"><img src=x onerror=1>' }), 'roster with hostile find');
    // The pane with nothing found says so, without a button.
    const blank = l.emptyHtml(state, { ...view, find: 'zzz' });
    assert.match(blank, /No one by that name\./);
    assert.doesNotMatch(blank, /data-action=/);
    assert.equal(l.selectedRow(state, { ...view, find: 'zzz' }), null, 'no row is selected when none shows');
    assert.match(l.emptyHtml(s.normalize({})), /No bonds yet\./, 'with no find text the empty state is as before');
    // The roster and the pane agree: no bonds at all, or every bond away and hidden, is not "No one by that name".
    const noBonds = s.normalize({});
    const allAway = s.normalize({ showAwayFollowers: false, romances: [{ name: 'A', isFollowing: false }] });
    for (const [st, line] of [[noBonds, /No bonds yet\./], [allAway, /Every bond is away\./]]) {
        const v = { ...s.createView(), find: 'zzz' };
        assert.doesNotMatch(l.rosterHtml(st, v), /No one by that name/, 'the roster does not say it');
        assert.doesNotMatch(l.emptyHtml(st, v), /No one by that name/, 'nor the pane');
        assert.match(l.emptyHtml(st, v), line, 'the pane keeps its own empty state');
    }
}

console.log('Ledger: ok');
