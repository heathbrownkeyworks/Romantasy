import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const s = require('../../view/js/state.js');
const f = require('../../view/js/format.js');
const settings = require('../../view/js/settings.js');

const sandbox = { window: {} };
vm.runInNewContext(readFileSync(new URL('../../view/dev/fixture.js', import.meta.url), 'utf8'), sandbox);
const state = s.normalize(sandbox.window.ROMANTASY_FIXTURE);

const SWITCHES = [
    ['openWithFavorites', 'Open with Favorites Menu', 'Off: open with Left Ctrl + R. On: cast the Romantasy power from your Favorites menu; the hotkey is disabled.'],
    ['showGainModals', '"Romance Deepened" popups', 'Show the gold modal when a bond crosses a tier upward.'],
    ['showLossModals', '"Romance Wounded" popups', 'Show the rose modal when a bond slips below a tier.'],
    ['showAwayFollowers', 'Show away companions', 'Keep bonds in the ledger even when not following.'],
    ['whispers', 'Whispers', 'Small notes on the HUD when a companion reacts. They never pause the game.'],
    ['highContrast', 'Stronger contrast', 'Brighter text, solid panels and thicker outlines.'],
    ['reduceMotion', 'Reduce motion', 'Stops the drifting backdrop and the larger animations.'],
    ['detailedLog', 'Detailed log', 'Writes every point change to Romantasy.log. For mod authors and bug reports.'],
    ['developerToolsUnlocked', 'Developer tools', 'Reveal testing utilities. Enabling prompts for a password.']
];
const DEV_BUTTONS = ['Bond +500', 'Bond −500', 'Side quest +1', 'Murder +1', 'Refresh'];
const NOTE = 'Testing utilities. Point adjustments and simulated deeds affect only actively following companions.';

// The header, and every switch's title and detail, escaped as the page escapes them.
const mixed = { ...state, openWithFavorites: true, showGainModals: false, showLossModals: true, showAwayFollowers: false, whispers: false,
    highContrast: true, reduceMotion: false, detailedLog: true, developerToolsUnlocked: false };
const locked = settings.paneHtml(mixed);
assert.ok(locked.includes('Sealed Page'));
assert.ok(locked.includes(f.esc("The scribe's quill")));
for (const [, title, detail] of SWITCHES) {
    assert.ok(locked.includes(f.esc(title)), `pane shows ${title}`);
    assert.ok(locked.includes(f.esc(detail)), `pane shows the detail of ${title}`);
}
assert.doesNotMatch(locked, /"Romance/, 'the quoted titles are escaped');

// Nine switches, each checked exactly as the state says, in order: Whispers, Stronger contrast, Reduce motion and
// Detailed log after "Show away companions" and before the developer tools.
const switches = html => [...html.matchAll(/<[^>]*role="switch"[^>]*>/g)].map(m => m[0]);
const checked = tag => /aria-checked="(true|false)"/.exec(tag)[1] === 'true';
const keyOf = tag => /data-key="([^"]*)"/.exec(tag)[1];
assert.equal(switches(locked).length, 9);
assert.deepEqual(switches(locked).map(keyOf), SWITCHES.map(([key]) => key));
SWITCHES.forEach(([key], i) => assert.equal(checked(switches(locked)[i]), mixed[key], `${key} switch`));
const flipped = settings.paneHtml({ ...mixed, openWithFavorites: false, showGainModals: true, showAwayFollowers: true, whispers: true, highContrast: false, reduceMotion: true, detailedLog: false });
assert.deepEqual(switches(flipped).map(checked), [false, true, true, true, true, false, true, false, false]);

// Three choice rows: Tier changes arrive, then Relationship pace, after "Show away companions"; Whisper corner after
// the Whispers switch; all before "Detailed log". Each is a radiogroup of buttons, one per option, the current one
// checked; the copy is verbatim. Whisper corner has a title and no detail line.
const CHOICES = [
    ['tierMoment', 'Tier changes arrive', [['calm', 'At a calm moment'], ['immediate', 'At once']],
        'A calm moment is an inn, a home, a town, or waking from sleep. A change never waits longer than a day.'],
    ['pace', 'Relationship pace', [['slow', 'Slow burn'], ['standard', 'Standard'], ['fast', 'Fast']],
        'Fast doubles the points from deeds. Slow burn halves them and asks for days together before each higher tier. Losses are never changed.'],
    ['whisperCorner', 'Whisper corner', [['topLeft', 'Top left'], ['topRight', 'Top right'], ['bottomLeft', 'Bottom left'], ['bottomRight', 'Bottom right']], ''],
    ['ledgerSize', 'Ledger size', [['standard', 'Standard'], ['large', 'Large'], ['largest', 'Largest']], 'Makes everything in the ledger bigger.']
];
const groupsOf = html => [...html.matchAll(/<div[^>]*role="radiogroup"[^>]*>[\s\S]*?<\/div>/g)].map(m => m[0]);
const radiosOf = group => [...group.matchAll(/<button([^>]*)>([^<]*)<\/button>/g)].map(m => ({ attrs: m[1], label: m[2] }));
const attr = (attrs, name) => { const m = new RegExp(name + '="([^"]*)"').exec(attrs); return m ? m[1] : null; };
const groups = groupsOf(locked);
assert.equal(groups.length, 4, 'four choice rows');
const at = key => locked.indexOf('data-key="' + key + '"');
assert.ok(at('showAwayFollowers') < at('tierMoment') && at('tierMoment') < at('pace') && at('pace') < at('detailedLog'),
    'after Show away companions, before Detailed log, Tier changes arrive first');
assert.ok(at('pace') < at('whispers') && at('whispers') < at('whisperCorner') && at('whisperCorner') < at('detailedLog'),
    'Whispers and Whisper corner after Relationship pace and before Detailed log');
// The comfort rows, together: Ledger size, Stronger contrast, Reduce motion, after Whisper corner and before Detailed log.
assert.ok(at('whisperCorner') < at('ledgerSize') && at('ledgerSize') < at('highContrast') && at('highContrast') < at('reduceMotion')
    && at('reduceMotion') < at('detailedLog'), 'Ledger size, Stronger contrast and Reduce motion between Whisper corner and Detailed log');
// Every row's --i counts the rows in order, so the entrance runs top to bottom.
assert.deepEqual([...locked.matchAll(/class="setrow[^"]*"[^>]*style="--i:(\d+)"/g)].map(m => Number(m[1])), [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]);
CHOICES.forEach(([key, title, options, detail], i) => {
    const group = groups[i];
    assert.ok(group.includes('<b class="title">' + f.esc(title) + '</b>'), title + ' title');
    if (detail) assert.ok(group.includes('<span class="detail">' + f.esc(detail) + '</span>'), title + ' detail');
    else assert.ok(!group.includes('class="detail"'), title + ' has no detail line');
    const radios = radiosOf(group);
    assert.deepEqual(radios.map(r => r.label), options.map(o => o[1]), title + ' options, in order');
    for (const [j, r] of radios.entries()) {
        assert.equal(attr(r.attrs, 'role'), 'radio');
        assert.equal(attr(r.attrs, 'data-action'), 'settings.choose');
        assert.equal(attr(r.attrs, 'data-key'), key);
        assert.equal(attr(r.attrs, 'data-value'), options[j][0]);
    }
    const checkedOf = st => radiosOf(groupsOf(settings.paneHtml(st))[i]).map(r => attr(r.attrs, 'aria-checked'));
    for (const [value] of options) {
        assert.deepEqual(checkedOf({ ...mixed, [key]: value }), options.map(o => o[0] === value ? 'true' : 'false'), key + ' = ' + value + ' is the one checked');
    }
});
assert.deepEqual(radiosOf(groupsOf(settings.paneHtml(s.normalize({})))[1]).map(r => attr(r.attrs, 'aria-checked')), ['false', 'true', 'false'], 'Standard by default');
assert.deepEqual(radiosOf(groupsOf(settings.paneHtml(s.normalize({})))[0]).map(r => attr(r.attrs, 'aria-checked')), ['true', 'false'], 'a calm moment by default');
assert.deepEqual(radiosOf(groupsOf(settings.paneHtml(s.normalize({})))[2]).map(r => attr(r.attrs, 'aria-checked')), ['false', 'true', 'false', 'false'], 'Top right by default');
assert.deepEqual(radiosOf(groupsOf(settings.paneHtml(s.normalize({})))[3]).map(r => attr(r.attrs, 'aria-checked')), ['true', 'false', 'false'], 'Standard size by default');
assert.deepEqual(switches(settings.paneHtml(s.normalize({}))).filter(t => ['highContrast', 'reduceMotion'].includes(keyOf(t))).map(checked), [false, false], 'contrast and motion off by default');

// The corner row is disabled, not hidden, while Whispers is off: the group says so and every option is a disabled
// button. The other choice rows never are.
const groupTag = group => /^<div[^>]*>/.exec(group)[0];
const isDisabled = r => /(^|\s)disabled(\s|$)/.test(r.attrs);
const cornerOff = groupsOf(settings.paneHtml({ ...mixed, whispers: false }))[2];
assert.equal(attr(groupTag(cornerOff), 'aria-disabled'), 'true', 'the corner group is aria-disabled while Whispers is off');
assert.equal(radiosOf(cornerOff).length, 4, 'and still drawn');
assert.ok(radiosOf(cornerOff).every(isDisabled), 'each corner option is disabled while Whispers is off');
const cornerOn = groupsOf(settings.paneHtml({ ...mixed, whispers: true }))[2];
assert.equal(attr(groupTag(cornerOn), 'aria-disabled'), null, 'the corner group is enabled while Whispers is on');
assert.ok(!radiosOf(cornerOn).some(isDisabled));
for (const st of [{ ...mixed, whispers: false }, { ...mixed, whispers: true }]) {
    for (const g of groupsOf(settings.paneHtml(st)).filter((_, i) => i !== 2)) {
        assert.equal(attr(groupTag(g), 'aria-disabled'), null);
        assert.ok(!radiosOf(g).some(isDisabled));
    }
}

// SkyrimNet: two switches after Whisper corner and before Detailed log, drawn only when SkyrimNet was found. Each is
// checked as skyrimNet.bridge and skyrimNet.conversation say; the conversation switch is disabled, not hidden, while the
// bridge is off. Without SkyrimNet there is no trace of either: no gap, no focus stop, the rows count as before.
const SKYRIMNET = [
    ['skyrimNetBridge', 'Tell SkyrimNet about bonds', "Adds each companion's bond to what SkyrimNet knows and reports tier changes to it."],
    ['skyrimNetConversation', 'Conversations can move the bond', "Lets SkyrimNet's AI nudge a bond by a few points for a kind or a harsh word. Capped each day."]
];
const net = (bridge, conversation, found = true) => ({ ...mixed, skyrimNet: { found, bridge, conversation } });
const tagOf = (html, key) => switches(html).find(tag => keyOf(tag) === key);
const tagDisabled = tag => /\sdisabled[\s>]/.test(tag);
for (const html of [locked, settings.paneHtml(net(true, true, false))]) {
    assert.ok(!/skyrimNet/i.test(html), 'nothing of SkyrimNet is drawn while it was not found');
}
const withNet = settings.paneHtml(net(true, false));
assert.deepEqual(switches(withNet).map(keyOf), ['openWithFavorites', 'showGainModals', 'showLossModals', 'showAwayFollowers', 'whispers',
    'skyrimNetBridge', 'skyrimNetConversation', 'highContrast', 'reduceMotion', 'detailedLog', 'developerToolsUnlocked']);
const atNet = key => withNet.indexOf('data-key="' + key + '"');
assert.ok(atNet('whisperCorner') < atNet('skyrimNetBridge') && atNet('skyrimNetBridge') < atNet('skyrimNetConversation')
    && atNet('skyrimNetConversation') < atNet('ledgerSize') && atNet('reduceMotion') < atNet('detailedLog'),
    'after Whisper corner, before the comfort rows and Detailed log');
for (const [, title, detail] of SKYRIMNET) {
    assert.ok(withNet.includes('<b class="title">' + f.esc(title) + '</b>'), title);
    assert.ok(withNet.includes('<span class="detail">' + f.esc(detail) + '</span>'), title + ' detail');
}
assert.deepEqual([...withNet.matchAll(/class="setrow[^"]*"[^>]*style="--i:(\d+)"/g)].map(m => Number(m[1])), [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]);
for (const [bridge, conversation] of [[true, true], [true, false], [false, true], [false, false]]) {
    const html = settings.paneHtml(net(bridge, conversation));
    assert.equal(checked(tagOf(html, 'skyrimNetBridge')), bridge);
    assert.equal(checked(tagOf(html, 'skyrimNetConversation')), conversation);
    assert.ok(!tagDisabled(tagOf(html, 'skyrimNetBridge')) && attr(tagOf(html, 'skyrimNetBridge'), 'aria-disabled') === null, 'the bridge switch is never disabled');
    assert.equal(tagDisabled(tagOf(html, 'skyrimNetConversation')), !bridge, 'the conversation switch is disabled while the bridge is off');
    assert.equal(attr(tagOf(html, 'skyrimNetConversation'), 'aria-disabled'), bridge ? null : 'true');
    for (const key of ['openWithFavorites', 'whispers', 'detailedLog']) assert.ok(!tagDisabled(tagOf(html, key)), key + ' is never disabled');
}

// OStim: two switches after the SkyrimNet rows and before Ledger size, drawn only when OStim was found. Each is checked
// as ostim.moments and ostim.hold say; the two are independent, so neither is ever disabled. Without OStim there is no
// trace of either.
const OSTIM = [
    ['ostimMoments', 'Shared moments', 'Time alone with a companion in an OStim scene adds a little to the bond, once a day.'],
    ['ostimHold', 'Quiet during scenes', 'Tier changes and whispers wait until an OStim scene is over.']
];
const os = (moments, hold, found = true, extra = {}) => ({ ...mixed, ...extra, ostim: { found, moments, hold } });
for (const html of [locked, settings.paneHtml(os(true, true, false)), settings.paneHtml(os(true, true, false, net(true, true)))]) {
    assert.ok(!/ostim/i.test(html), 'nothing of OStim is drawn while it was not found');
}
const withOStim = settings.paneHtml(os(true, false));
assert.deepEqual(switches(withOStim).map(keyOf), ['openWithFavorites', 'showGainModals', 'showLossModals', 'showAwayFollowers', 'whispers',
    'ostimMoments', 'ostimHold', 'highContrast', 'reduceMotion', 'detailedLog', 'developerToolsUnlocked']);
const atOs = key => withOStim.indexOf('data-key="' + key + '"');
assert.ok(atOs('whisperCorner') < atOs('ostimMoments') && atOs('ostimMoments') < atOs('ostimHold') && atOs('ostimHold') < atOs('ledgerSize'),
    'after Whisper corner, before Ledger size');
for (const [, title, detail] of OSTIM) {
    assert.ok(withOStim.includes('<b class="title">' + f.esc(title) + '</b>'), title);
    assert.ok(withOStim.includes('<span class="detail">' + f.esc(detail) + '</span>'), title + ' detail');
}
assert.deepEqual([...withOStim.matchAll(/class="setrow[^"]*"[^>]*style="--i:(\d+)"/g)].map(m => Number(m[1])), [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]);
// With SkyrimNet too: the OStim rows follow the SkyrimNet rows.
const both = settings.paneHtml(os(true, true, true, net(true, true)));
assert.deepEqual(switches(both).map(keyOf), ['openWithFavorites', 'showGainModals', 'showLossModals', 'showAwayFollowers', 'whispers',
    'skyrimNetBridge', 'skyrimNetConversation', 'ostimMoments', 'ostimHold', 'highContrast', 'reduceMotion', 'detailedLog', 'developerToolsUnlocked']);
const atBoth = key => both.indexOf('data-key="' + key + '"');
assert.ok(atBoth('skyrimNetConversation') < atBoth('ostimMoments') && atBoth('ostimHold') < atBoth('ledgerSize'), 'after SkyrimNet, before Ledger size');
for (const [moments, hold] of [[true, true], [true, false], [false, true], [false, false]]) {
    const html = settings.paneHtml(os(moments, hold));
    assert.equal(checked(tagOf(html, 'ostimMoments')), moments);
    assert.equal(checked(tagOf(html, 'ostimHold')), hold);
    for (const key of ['ostimMoments', 'ostimHold']) {
        assert.ok(!tagDisabled(tagOf(html, key)) && attr(tagOf(html, key), 'aria-disabled') === null, key + ' is never disabled');
    }
}

// Locked: no developer buttons and no note. Unlocked: exactly the five buttons and the note.
const buttonLabels = html => [...html.matchAll(/<button[^>]*data-action="settings\.dev-[^"]*"[^>]*>([^<]*)<\/button>/g)].map(m => m[1]);
assert.deepEqual(buttonLabels(locked), []);
assert.ok(!locked.includes(NOTE));
for (const label of DEV_BUTTONS) assert.ok(!locked.includes('>' + label + '<'), `${label} hidden while locked`);
const unlocked = settings.paneHtml({ ...mixed, developerToolsUnlocked: true });
assert.deepEqual(buttonLabels(unlocked), DEV_BUTTONS);
assert.ok(unlocked.includes(NOTE));
assert.equal(checked(switches(unlocked)[8]), true);

// A way back to the dossier.
assert.match(locked, /data-action="settings\.back"/);

/* ---------- Health ---------- */
const OK_LINE = 'Everything is in order.';
const withHealth = health => s.normalize({ ...sandbox.window.ROMANTASY_FIXTURE, health });
const healthOf = html => { const i = html.indexOf('class="healthcard'); return i < 0 ? '' : html.slice(html.lastIndexOf('<section', i)); };
const levels = html => [...healthOf(html).matchAll(/data-level="([^"]*)"/g)].map(m => m[1]);

// Healthy: the line, the versions and the bond count; no item rows.
const healthy = settings.paneHtml(withHealth({ status: 'ok', version: '3.0.1', host: '1.6.2', bonds: 2, items: [] }));
assert.match(healthOf(healthy), /<h3 class="eyebrow">Health<\/h3>/);
assert.ok(healthOf(healthy).includes(OK_LINE));
assert.ok(healthOf(healthy).includes('Romantasy 3.0.1 · Magelight UI 1.6.2 · 2 bonds'));
assert.deepEqual(levels(healthy), []);
assert.ok(settings.paneHtml(withHealth({ status: 'ok', version: '3.0.1', host: '1.6.2', bonds: 1, items: [] })).includes('Romantasy 3.0.1 · Magelight UI 1.6.2 · 1 bond<'));
// No host version: the Magelight part is left out, not printed empty.
const noHost = healthOf(settings.paneHtml(withHealth({ status: 'ok', version: '3.0.1', host: '', bonds: 0, items: [] })));
assert.ok(noHost.includes('Romantasy 3.0.1 · 0 bonds'));
assert.ok(!noHost.includes('Magelight UI'));
// Healthy: Health sits at the foot of the pane, after the last switch row.
assert.ok(healthy.indexOf('class="healthcard') > healthy.lastIndexOf('role="switch"'));
const unlockedHealthy = settings.paneHtml({ ...withHealth({ status: 'ok', version: '3.0.1', host: '1.6.2', bonds: 2, items: [] }), developerToolsUnlocked: true });
assert.ok(unlockedHealthy.indexOf('class="healthcard') > unlockedHealthy.indexOf('class="devcard'), 'Health stays below the developer card');

// Error, warn and note: three rows in that order, each with its level, title and detail lines.
const troubled = withHealth({
    status: 'error', version: '3.0.1', host: '1.6.2', bonds: 2, items: [
        { id: 'conditions', level: 'error', title: 'Dialogue conditions are off', detail: 'This game version is not supported by the dialogue bridge. Romance dialogue may not unlock.' },
        { id: 'profiles', level: 'warn', title: '3 personality file(s) could not be used',
            detail: 'Some Long File Name.toml: schema_version must be integer 1\nAnother.toml: schema_version must be integer 1\nThird.toml: schema_version must be integer 1' },
        { id: 'orphans', level: 'note', title: 'Keeping 2 bond(s) for followers whose mods are not loaded', detail: 'They return when the mods do. Nothing is lost.' }
    ]
});
const troubledPane = settings.paneHtml(troubled);
const troubledHtml = healthOf(troubledPane);
// Problems: Health comes first in the list, before the first switch row (and the developer card).
assert.ok(troubledPane.indexOf('class="healthcard') >= 0);
assert.ok(troubledPane.indexOf('class="healthcard') < troubledPane.indexOf('role="switch"'), 'warn or error: Health comes first');
assert.match(troubledPane, /<div class="setlist scroller"><section class="healthcard/);
const warnPane = settings.paneHtml({ ...withHealth({ status: 'warn', items: [{ id: 'p', level: 'warn', title: 'T', detail: 'D' }] }), developerToolsUnlocked: true });
assert.ok(warnPane.indexOf('class="healthcard') < warnPane.indexOf('role="switch"'));
assert.equal((warnPane.match(/class="healthcard/g) || []).length, 1, 'drawn once');
assert.deepEqual(levels(troubledHtml), ['error', 'warn', 'note']);
assert.ok(!troubledHtml.includes(OK_LINE));
const titles = [...troubledHtml.matchAll(/<b class="htitle">(?:<span class="hword">[^<]*<\/span>)?([^<]*)<\/b>/g)].map(m => m[1]);
assert.deepEqual(titles, ['Dialogue conditions are off', '3 personality file(s) could not be used', 'Keeping 2 bond(s) for followers whose mods are not loaded']);
const words = [...troubledHtml.matchAll(/<span class="hword">([^<]*)<\/span>/g)].map(m => m[1]);
assert.deepEqual(words, ['Problem', 'Check'], 'error says Problem, warn says Check, a note has no word');
const detailLines = [...troubledHtml.matchAll(/<span class="hline">([^<]*)<\/span>/g)].map(m => m[1]);
assert.deepEqual(detailLines, [
    'This game version is not supported by the dialogue bridge. Romance dialogue may not unlock.',
    'Some Long File Name.toml: schema_version must be integer 1', 'Another.toml: schema_version must be integer 1', 'Third.toml: schema_version must be integer 1',
    'They return when the mods do. Nothing is lost.'
]);

// Notes only: still in order, and the note is listed under the line.
const noted = healthOf(settings.paneHtml(withHealth({ status: 'ok', version: '3.0.1', host: '1.6.2', bonds: 2,
    items: [{ id: 'orphans', level: 'note', title: 'Keeping 2 bond(s) for followers whose mods are not loaded', detail: 'They return when the mods do. Nothing is lost.' }] })));
assert.ok(noted.includes(OK_LINE));
assert.deepEqual(levels(noted), ['note']);
assert.ok(noted.indexOf(OK_LINE) < noted.indexOf('data-level="note"'));
const notedPane = settings.paneHtml(withHealth({ status: 'ok', version: '3.0.1', host: '1.6.2', bonds: 2,
    items: [{ id: 'orphans', level: 'note', title: 'N', detail: 'D' }] }));
assert.ok(notedPane.indexOf('class="healthcard') > notedPane.lastIndexOf('role="switch"'), 'notes only: Health stays at the foot');

// The status is the one source: warn with only a note item is not calm, comes first and marks the control.
const disagreeing = withHealth({ status: 'warn', version: '3.0.1', host: '1.6.2', bonds: 2,
    items: [{ id: 'orphans', level: 'note', title: 'N', detail: 'D' }] });
const disagreeingPane = settings.paneHtml(disagreeing);
assert.ok(!disagreeingPane.includes(OK_LINE), 'status warn never says everything is in order');
assert.match(disagreeingPane, /<div class="setlist scroller"><section class="healthcard/);
assert.match(require('../../view/js/ledger.js').rosterHtml(disagreeing, s.createView()), /class="mark"/);

// No health in the state: no section, nothing thrown.
assert.equal(healthOf(settings.paneHtml(state)), '');
assert.equal(healthOf(settings.paneHtml(s.normalize({}))), '');
assert.equal(healthOf(settings.paneHtml(withHealth('nonsense'))), '');

// Text from the state goes in as text.
const evil = healthOf(settings.paneHtml(withHealth({ status: 'warn', version: '<i>3</i>', host: '1.6.2', bonds: 1,
    items: [{ id: 'x', level: 'warn', title: 'A <b>bold</b> title', detail: '<img src=x>' }] })));
assert.ok(evil.includes('A &lt;b&gt;bold&lt;/b&gt; title'));
assert.ok(evil.includes('&lt;img src=x&gt;'));
assert.ok(!evil.includes('<b>bold') && !evil.includes('<img src=x'));

// The control that opens Settings carries a mark while the status is warn or error.
const ledger = require('../../view/js/ledger.js');
const settingsButton = html => /<button[^>]*data-action="settings"[^>]*>[\s\S]*?<\/button>/.exec(html)[0];
const markOf = health => settingsButton(ledger.rosterHtml(withHealth(health), s.createView()));
assert.match(markOf({ status: 'warn', items: [] }), /<span class="mark" data-level="warn" role="img" aria-label="Health: Check"><\/span>/);
assert.match(markOf({ status: 'error', items: [] }), /<span class="mark" data-level="error" role="img" aria-label="Health: Problem"><\/span>/);
assert.doesNotMatch(markOf({ status: 'ok', items: [] }), /class="mark"/);
assert.doesNotMatch(settingsButton(ledger.rosterHtml(state, s.createView())), /class="mark"/);

/* ---------- the page wiring, under a minimal stand-in DOM ---------- */
function element() {
    const attrs = {}, classes = new Set();
    return {
        innerHTML: '', className: '', textContent: '', value: '', style: {}, offsetWidth: 0, isConnected: true, disabled: false,
        classList: { add: c => classes.add(c), remove: c => classes.delete(c), toggle: (c, on) => (on ?? !classes.has(c)) ? classes.add(c) : classes.delete(c), contains: c => classes.has(c) },
        setAttribute: (k, v) => { attrs[k] = String(v); }, getAttribute: k => (k in attrs ? attrs[k] : null), removeAttribute: k => { delete attrs[k]; },
        querySelector: () => null, querySelectorAll: () => [], addEventListener() {}, focus() {}, appendChild() {}, remove() {}
    };
}
// Objects made inside the page's context have that context's prototypes: compare plain copies.
const plain = v => v === undefined || v === null ? v : JSON.parse(JSON.stringify(v));
const elements = new Map();
const sent = [];
const topics = new Map();
const page = {
    console, setTimeout, clearTimeout,
    document: {
        readyState: 'complete', documentElement: element(),
        getElementById: id => { if (!elements.has(id)) elements.set(id, element()); return elements.get(id); },
        addEventListener() {}, createElement: element, querySelector: () => null
    }
};
page.window = page;
page.RM = {
    fit: { apply() {} },
    bridge: {
        focused: true,
        on: (topic, fn) => topics.set(topic, fn),
        send: (name, payload) => { sent.push([name, plain(payload)]); return true; },
        sound() {}
    }
};
for (const file of ['strings.js', 'tiers.js', 'format.js', 'state.js', 'notice.js', 'roster.js', 'ledger.js', 'settings.js', 'modals.js', 'app.js']) {
    vm.runInNewContext(readFileSync(new URL('../../view/js/' + file, import.meta.url), 'utf8'), page);
}
const RM = page.RM, app = RM.app;
const part = name => RM.parts.find(p => p.name === name);
const button = attrs => { const el = element(); for (const k in attrs) el.setAttribute(k, attrs[k]); return el; };
const act = (name, attrs) => { for (const p of RM.parts) if (p.actions && p.actions[name]) return p.actions[name](button(attrs || {})); throw new Error('no action ' + name); };

topics.get('romantasyState')(JSON.parse(JSON.stringify(sandbox.window.ROMANTASY_FIXTURE)));
const rows = s.visibleRows(app.state);

// Settings opens from the ledger's event; selecting a roster row returns the pane to the dossier.
app.emit('open-settings');
assert.equal(app.view.pane, 'settings');
act('select', { 'data-identity': s.rowIdentity(rows[2]) });
assert.equal(app.view.pane, 'dossier', 'selecting a row leaves settings');
assert.equal(app.view.selected, s.rowIdentity(rows[2]));
app.emit('open-settings');
act('settings.back');
assert.equal(app.view.pane, 'dossier', 'Back returns to the dossier');

// A switch sends its one key with the new value.
sent.length = 0;
act('settings.toggle', { 'data-key': 'showGainModals' });
act('settings.toggle', { 'data-key': 'openWithFavorites' });
act('settings.toggle', { 'data-key': 'detailedLog' });
assert.deepEqual(sent, [['romantasySettings', { showGainModals: false }], ['romantasySettings', { openWithFavorites: true }], ['romantasySettings', { detailedLog: true }]]);

// A choice sends its one key with the chosen value; the current option sends nothing; an unknown one nothing.
sent.length = 0;
app.state = { ...app.state, pace: 'standard', tierMoment: 'calm' };
act('settings.choose', { 'data-key': 'pace', 'data-value': 'fast' });
act('settings.choose', { 'data-key': 'tierMoment', 'data-value': 'immediate' });
act('settings.choose', { 'data-key': 'pace', 'data-value': 'standard' });
act('settings.choose', { 'data-key': 'tierMoment', 'data-value': 'calm' });
act('settings.choose', { 'data-key': 'pace', 'data-value': 'turbo' });
act('settings.choose', { 'data-key': 'showGainModals', 'data-value': 'true' });
assert.deepEqual(sent, [['romantasySettings', { pace: 'fast' }], ['romantasySettings', { tierMoment: 'immediate' }]]);

// Whispers is a switch; a corner is a choice, and sends nothing while Whispers is off.
sent.length = 0;
app.state = { ...app.state, whispers: true, whisperCorner: 'topRight' };
act('settings.toggle', { 'data-key': 'whispers' });
act('settings.choose', { 'data-key': 'whisperCorner', 'data-value': 'bottomLeft' });
act('settings.choose', { 'data-key': 'whisperCorner', 'data-value': 'topRight' });
act('settings.choose', { 'data-key': 'whisperCorner', 'data-value': 'middle' });
app.state = { ...app.state, whispers: false };
act('settings.toggle', { 'data-key': 'whispers' });
act('settings.choose', { 'data-key': 'whisperCorner', 'data-value': 'topLeft' });
assert.deepEqual(sent, [['romantasySettings', { whispers: false }], ['romantasySettings', { whisperCorner: 'bottomLeft' }],
    ['romantasySettings', { whispers: true }]]);

// The comfort rows send their one key: Ledger size its value (the current one and an unknown one nothing), the two switches the new value.
sent.length = 0;
app.state = { ...app.state, ledgerSize: 'standard', highContrast: false, reduceMotion: false };
act('settings.choose', { 'data-key': 'ledgerSize', 'data-value': 'largest' });
act('settings.choose', { 'data-key': 'ledgerSize', 'data-value': 'large' });
act('settings.choose', { 'data-key': 'ledgerSize', 'data-value': 'standard' });
act('settings.choose', { 'data-key': 'ledgerSize', 'data-value': 'huge' });
act('settings.toggle', { 'data-key': 'highContrast' });
act('settings.toggle', { 'data-key': 'reduceMotion' });
app.state = { ...app.state, highContrast: true, reduceMotion: true };
act('settings.toggle', { 'data-key': 'highContrast' });
act('settings.toggle', { 'data-key': 'reduceMotion' });
assert.deepEqual(sent, [['romantasySettings', { ledgerSize: 'largest' }], ['romantasySettings', { ledgerSize: 'large' }],
    ['romantasySettings', { highContrast: true }], ['romantasySettings', { reduceMotion: true }],
    ['romantasySettings', { highContrast: false }], ['romantasySettings', { reduceMotion: false }]]);

// The SkyrimNet switches send their one key, read from skyrimNet; the conversation switch sends nothing while the
// bridge is off, and neither sends anything while SkyrimNet was not found.
sent.length = 0;
app.state = { ...app.state, skyrimNet: { found: true, bridge: true, conversation: true } };
act('settings.toggle', { 'data-key': 'skyrimNetBridge' });
act('settings.toggle', { 'data-key': 'skyrimNetConversation' });
app.state = { ...app.state, skyrimNet: { found: true, bridge: false, conversation: false } };
act('settings.toggle', { 'data-key': 'skyrimNetConversation' });
act('settings.toggle', { 'data-key': 'skyrimNetBridge' });
app.state = { ...app.state, skyrimNet: { found: false, bridge: true, conversation: true } };
act('settings.toggle', { 'data-key': 'skyrimNetBridge' });
act('settings.toggle', { 'data-key': 'skyrimNetConversation' });
assert.deepEqual(sent, [['romantasySettings', { skyrimNetBridge: false }], ['romantasySettings', { skyrimNetConversation: false }],
    ['romantasySettings', { skyrimNetBridge: true }]]);

// The OStim switches send their one key, read from ostim, whatever the other says; nothing while OStim was not found.
sent.length = 0;
app.state = { ...app.state, ostim: { found: true, moments: true, hold: true } };
act('settings.toggle', { 'data-key': 'ostimMoments' });
act('settings.toggle', { 'data-key': 'ostimHold' });
app.state = { ...app.state, ostim: { found: true, moments: false, hold: false } };
act('settings.toggle', { 'data-key': 'ostimHold' });
act('settings.toggle', { 'data-key': 'ostimMoments' });
app.state = { ...app.state, ostim: { found: false, moments: true, hold: true } };
act('settings.toggle', { 'data-key': 'ostimMoments' });
act('settings.toggle', { 'data-key': 'ostimHold' });
assert.deepEqual(sent, [['romantasySettings', { ostimMoments: false }], ['romantasySettings', { ostimHold: false }],
    ['romantasySettings', { ostimHold: true }], ['romantasySettings', { ostimMoments: true }]]);

// Developer tools: off -> on asks for the password; on -> off locks.
sent.length = 0;
act('settings.toggle', { 'data-key': 'developerToolsUnlocked' });
assert.deepEqual(plain(app.view.modal), { kind: 'password' });
assert.deepEqual(sent, []);
app.view.modal = null;
app.state = { ...app.state, developerToolsUnlocked: true };
act('settings.toggle', { 'data-key': 'developerToolsUnlocked' });
assert.deepEqual(sent, [['romantasyDevLock', undefined]]);

// The developer buttons.
sent.length = 0;
for (const name of ['settings.dev-bond-up', 'settings.dev-bond-down', 'settings.dev-side-quest', 'settings.dev-murder', 'settings.dev-refresh']) act(name);
assert.deepEqual(sent, [
    ['romantasyDebug', { points: 500 }], ['romantasyDebug', { points: -500 }],
    ['romantasyDebug', { stat: 'Side Quests Completed', delta: 1 }], ['romantasyDebug', { stat: 'Murders', delta: 1 }],
    ['romantasyRefresh', undefined]
]);

// The ledger's confirm event opens the modal; its buttons send the bond request and close.
sent.length = 0;
const player = app.state.romances.find(r => r.removable), id = s.rowIdentity(player);
app.emit('confirm', { kind: 'remove', identity: id });
assert.deepEqual(plain(app.view.modal), { kind: 'remove', identity: id });
act('modal.confirm');
assert.deepEqual(sent, [['romantasyBond', { kind: 'remove', identity: id }]]);
assert.equal(app.view.modal, null);
app.emit('confirm', { kind: 'reset', identity: id });
act('modal.cancel');
assert.equal(app.view.modal, null);
assert.equal(sent.length, 1, 'Cancel sends nothing');

// The password modal: Unlock sends the password and waits; the next state closes it or shows the error.
sent.length = 0;
app.state = { ...app.state, developerToolsUnlocked: false };
app.view.modal = { kind: 'password' };
RM.modals.submit('wrong');
assert.deepEqual(sent, [['romantasyDevUnlock', { password: 'wrong' }]]);
assert.equal(app.view.modal.pending, true);
topics.get('romantasyState')({ ...sandbox.window.ROMANTASY_FIXTURE, developerToolsUnlocked: false });
assert.equal(app.view.modal.kind, 'password');
assert.equal(app.view.modal.error, true);
assert.equal(app.view.modal.pending, false);
RM.modals.submit('mock');
topics.get('romantasyState')({ ...sandbox.window.ROMANTASY_FIXTURE, developerToolsUnlocked: true });
assert.equal(app.view.modal, null, 'a successful unlock closes the modal');
RM.modals.submit('ignored');   // no modal open: nothing is sent
assert.equal(sent.length, 2);

// The notice records the revision it showed.
assert.equal(app.view.lastRevision, app.state.revision);
assert.equal(typeof part('notice').render, 'function');

// Closing the ledger drops the open modal and the settings pane once __uimode stays off.
app.emit('open-settings');
app.view.modal = { kind: 'reset', identity: id };
RM.bridge.focused = false;
topics.get('__uimode')(false);
await new Promise(resolve => setTimeout(resolve, 320));
assert.equal(app.view.modal, null, 'the modal is cleared');
assert.equal(app.view.pane, 'dossier', 'the settings pane is cleared');

console.log('Settings: ok');
