import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const s = createRequire(import.meta.url)('../../view/js/state.js');

assert.deepEqual(s.normalize(null).romances, []);
assert.equal(s.normalize({}).showGainModals, true);
const n = s.normalize({ romances: [{ name: 'A', points: '12x' }, 7, null, { name: 'B', points: 1240, likes: 'x' }] });
assert.equal(n.romances.length, 2);
assert.equal(n.romances[0].points, 0); assert.equal(n.romances[0].levelName, 'Stranger');
assert.equal(n.romances[1].levelName, 'Friend'); assert.deepEqual(n.romances[1].likes, []);
assert.equal(s.rowIdentity({ referenceFormID: 'AD005901', baseFormID: 'AD005900' }), 'AD005901');
assert.equal(s.rowIdentity({ referenceFormID: '00000000', baseFormID: 'AD005900' }), 'AD005900');
assert.equal(s.rowIdentity({ referenceFormID: '', baseFormID: 'AD005900' }), 'AD005900');
const rows = ['A', 'B', 'C'].map(id => ({ referenceFormID: id, isFollowing: id !== 'B' }));
assert.equal(s.visibleRows({ romances: rows, showAwayFollowers: false }).length, 2);
assert.equal(s.visibleRows({ romances: rows, showAwayFollowers: true }).length, 3);
assert.equal(s.reconcileSelection('B', 1, rows), 'B');
assert.equal(s.reconcileSelection('B', 1, [rows[0], rows[2]]), 'C');
assert.equal(s.reconcileSelection('C', 2, [rows[0], rows[1]]), 'B');
assert.equal(s.reconcileSelection('A', 0, []), null);
assert.equal(s.reconcileSelection(null, 0, rows), 'A');

// Every key present; provided values kept; a provided levelName is kept.
const d = s.normalize(null);
assert.deepEqual(Object.keys(d).sort(), [
    'detailedLog', 'developerToolsUnlocked', 'enrollmentCandidates', 'health', 'highContrast', 'ledgerSize', 'message', 'openWithFavorites', 'ostim', 'pace', 'preferenceOptions', 'reduceMotion', 'revision', 'romances', 'rosterOrder', 'showAwayFollowers', 'showGainModals', 'showLossModals', 'skyrimNet', 'tierMoment', 'whisperCorner', 'whispers']);
assert.equal(d.detailedLog, false);
assert.equal(d.pace, 'standard');
assert.equal(d.tierMoment, 'calm');
assert.equal(s.normalize({ pace: 'fast' }).pace, 'fast');
assert.equal(s.normalize({ pace: 'slow', tierMoment: 'immediate' }).tierMoment, 'immediate');
assert.equal(s.normalize({ pace: 'Fast' }).pace, 'standard');
assert.equal(s.normalize({ pace: 3, tierMoment: 'later' }).pace, 'standard');
assert.equal(s.normalize({ tierMoment: 'later' }).tierMoment, 'calm');
// Whispers: on and in the top right corner unless the state says otherwise.
assert.equal(d.whispers, true);
assert.equal(d.whisperCorner, 'topRight');

// Roster order, ledger size, contrast and motion: the defaults unless the state says otherwise, exactly.
assert.equal(d.rosterOrder, 'closest');
assert.equal(d.ledgerSize, 'standard');
assert.equal(d.highContrast, false);
assert.equal(d.reduceMotion, false);
for (const v of ['closest', 'name', 'recent']) assert.equal(s.normalize({ rosterOrder: v }).rosterOrder, v);
for (const bad of ['Name', 'alphabetical', '', 2, null]) assert.equal(s.normalize({ rosterOrder: bad }).rosterOrder, 'closest');
for (const v of ['standard', 'large', 'largest']) assert.equal(s.normalize({ ledgerSize: v }).ledgerSize, v);
for (const bad of ['Large', 'huge', '', 1, null]) assert.equal(s.normalize({ ledgerSize: bad }).ledgerSize, 'standard');
for (const key of ['highContrast', 'reduceMotion']) {
    assert.equal(s.normalize({ [key]: true })[key], true);
    for (const bad of ['true', 1, null]) assert.equal(s.normalize({ [key]: bad })[key], false);
}
assert.equal(s.normalize({ whispers: false }).whispers, false);
assert.equal(s.normalize({ whispers: 'false' }).whispers, true);
assert.equal(s.normalize({ whispers: 0 }).whispers, true);
for (const corner of ['topLeft', 'topRight', 'bottomLeft', 'bottomRight']) assert.equal(s.normalize({ whisperCorner: corner }).whisperCorner, corner);
for (const bad of ['TopLeft', 'middle', '', 2, null]) assert.equal(s.normalize({ whisperCorner: bad }).whisperCorner, 'topRight');
assert.equal(s.normalize({ detailedLog: true }).detailedLog, true);

// SkyrimNet: not found unless the state says exactly true; both switches on unless the state says exactly false.
assert.deepEqual(d.skyrimNet, { found: false, bridge: true, conversation: true });
assert.deepEqual(s.normalize({ skyrimNet: { found: true, bridge: false, conversation: false } }).skyrimNet, { found: true, bridge: false, conversation: false });
assert.deepEqual(s.normalize({ skyrimNet: { found: true } }).skyrimNet, { found: true, bridge: true, conversation: true });
for (const bad of [1, 'true', null, undefined, {}]) assert.equal(s.normalize({ skyrimNet: { found: bad } }).skyrimNet.found, false);
for (const bad of [0, 'false', null]) {
    assert.equal(s.normalize({ skyrimNet: { found: true, bridge: bad } }).skyrimNet.bridge, true);
    assert.equal(s.normalize({ skyrimNet: { found: true, conversation: bad } }).skyrimNet.conversation, true);
}
for (const bad of [null, 'yes', true, 3, ['found']]) assert.deepEqual(s.normalize({ skyrimNet: bad }).skyrimNet, { found: false, bridge: true, conversation: true });

// OStim: not found unless the state says exactly true; both switches on unless the state says exactly false.
assert.deepEqual(d.ostim, { found: false, moments: true, hold: true });
assert.deepEqual(s.normalize({ ostim: { found: true, moments: false, hold: false } }).ostim, { found: true, moments: false, hold: false });
assert.deepEqual(s.normalize({ ostim: { found: true } }).ostim, { found: true, moments: true, hold: true });
for (const bad of [1, 'true', null, undefined, {}]) assert.equal(s.normalize({ ostim: { found: bad } }).ostim.found, false);
for (const bad of [0, 'false', null]) {
    assert.equal(s.normalize({ ostim: { found: true, moments: bad } }).ostim.moments, true);
    assert.equal(s.normalize({ ostim: { found: true, hold: bad } }).ostim.hold, true);
}
for (const bad of [null, 'yes', true, 3, ['found']]) assert.deepEqual(s.normalize({ ostim: bad }).ostim, { found: false, moments: true, hold: true });

// Health: absent or malformed is null; items keep their order; unknown levels and statuses fall back.
assert.equal(d.health, null);
assert.equal(s.normalize({ health: [] }).health, null);
const h = s.normalize({ health: { status: 'bad', version: 3, host: '1.6.2', bonds: -1, items: [
    { id: 'a', level: 'error', title: 'T', detail: 'D' }, 7, { id: 'b', level: 'odd', title: 5 }] } }).health;
assert.deepEqual(h, { status: 'ok', version: '', host: '1.6.2', bonds: 0, items: [
    { id: 'a', level: 'error', title: 'T', detail: 'D' }, { id: 'b', level: 'note', title: '', detail: '' }] });
assert.equal(s.healthAlert(s.normalize({ health: { status: 'warn' } })), true);
assert.equal(s.healthAlert(s.normalize({ health: { status: 'error' } })), true);
assert.equal(s.healthAlert(s.normalize({ health: { status: 'ok' } })), false);
assert.equal(s.healthAlert(s.normalize({})), false);
assert.equal(d.revision, 0); assert.equal(d.message, '');
const k = s.normalize({ revision: 4, showGainModals: false, romances: [{ points: 1240, levelName: 'Custom', isFollowing: true }, { points: 1240, levelName: '' }] });
assert.equal(k.revision, 4); assert.equal(k.showGainModals, false);
assert.equal(k.romances[0].levelName, 'Custom'); assert.equal(k.romances[0].isFollowing, true);
assert.equal(k.romances[1].levelName, 'Friend');
assert.equal(s.normalize({ romances: [{ points: Infinity }] }).romances[0].points, 0);
assert.deepEqual(Object.keys(k.romances[0]).sort(), ['baseFormID', 'dislikes', 'favorites', 'gate', 'isFollowing', 'levelName', 'likes', 'mood', 'name', 'personalityEditable',
    'petPeeves', 'points', 'profileFile', 'profileOrigin', 'recent', 'referenceFormID', 'removable', 'role', 'sourcePlugin', 'unlocks']);

// A recent change's game day: a finite number is kept, anything else is null; the entry keeps its other fields.
const dayOf = day => s.normalize({ romances: [{ recent: [{ label: 'a', points: 1, when: 'Today', day }] }] }).romances[0].recent[0];
assert.deepEqual(dayOf(120.5), { label: 'a', points: 1, when: 'Today', day: 120.5 });
assert.equal(dayOf(0).day, 0);
assert.equal(dayOf(-3).day, -3, 'a finite number is kept as it is (the roster passes over a negative one)');
for (const bad of [undefined, null, NaN, Infinity, -Infinity, '120', true, {}, []]) assert.equal(dayOf(bad).day, null, 'no day from ' + String(bad));
assert.equal(s.normalize({ romances: [{ recent: [{ label: 'a' }] }] }).romances[0].recent[0].day, null, 'an older plugin sends none');

// Mood: one of the three keys, else '' (a row from an older plugin has none; nothing else reaches a class name).
assert.deepEqual(s.normalize({ romances: [{ mood: 'warm' }, { mood: 'settled' }, { mood: 'strained' }, {}, { mood: 'Warm' },
    { mood: 'warm x' }, { mood: '"><b>' }, { mood: 3 }, { mood: null }] }).romances.map(r => r.mood),
['warm', 'settled', 'strained', '', '', '', '', '', '']);

// The Slow burn gate: kept only with an integer level 3 (Friend) to 6 (Spouse) and finite day counts, else null.
const gateOf = gate => s.normalize({ romances: [gate === undefined ? {} : { gate }] }).romances[0].gate;
assert.equal(gateOf(undefined), null, 'a row without a gate has none');
assert.deepEqual(gateOf({ level: 3, name: 'Friend', daysNeeded: 3, daysTogether: 1.46 }), { level: 3, name: 'Friend', daysNeeded: 3, daysTogether: 1.46 });
assert.deepEqual(gateOf({ level: 6, name: 'Spouse', daysNeeded: 21, daysTogether: 0 }), { level: 6, name: 'Spouse', daysNeeded: 21, daysTogether: 0 });
assert.equal(gateOf({ level: 3, name: 'Friend', daysNeeded: 0, daysTogether: 1 }), null, 'a gate that needs no days is no gate');
assert.equal(gateOf({ level: 3, name: 'Friend', daysNeeded: 3, daysTogether: -0.5 }), null, 'days together cannot be negative');
assert.deepEqual(gateOf({ level: 4, name: 7, daysNeeded: 7, daysTogether: 2 }), { level: 4, name: 'Confidant', daysNeeded: 7, daysTogether: 2 }, 'no name: the tier name');
for (const bad of [null, 'x', [], { level: 2, name: 'Acquaintance', daysNeeded: 1, daysTogether: 0 }, { level: 7, name: 'X', daysNeeded: 1, daysTogether: 0 },
    { level: 3.5, name: 'Friend', daysNeeded: 3, daysTogether: 1 }, { level: '3', name: 'Friend', daysNeeded: 3, daysTogether: 1 },
    { level: 3, name: 'Friend', daysNeeded: '3', daysTogether: 1 }, { level: 3, name: 'Friend', daysNeeded: 3, daysTogether: NaN },
    { level: 3, name: 'Friend', daysNeeded: Infinity, daysTogether: 1 }, { level: 3, name: 'Friend', daysNeeded: 3 }]) {
    assert.equal(gateOf(bad), null, 'no gate from ' + JSON.stringify(bad));
}

// Favorites, pet peeves and unlock lines: lists; an unlock keeps only integer levels 2 to 6 and string items.
const bare = k.romances[0];
assert.deepEqual([bare.favorites, bare.petPeeves, bare.unlocks], [[], [], []], 'a row without them gives three empty lists');
const rich = s.normalize({ romances: [{ favorites: ['A', 7, 'B'], petPeeves: ['C'], unlocks: [
    { level: 2, items: ['x', 3, 'y'] }, { level: 1, items: ['low'] }, { level: 7, items: ['high'] }, { level: 3.5, items: ['frac'] },
    { level: '4', items: ['text'] }, null, { level: 6, items: 'z' }, { level: 5, items: ['w'] }] }] }).romances[0];
assert.deepEqual(rich.favorites, ['A', 'B']);
assert.deepEqual(rich.petPeeves, ['C']);
assert.deepEqual(rich.unlocks, [{ level: 2, items: ['x', 'y'] }, { level: 6, items: [] }, { level: 5, items: ['w'] }]);
assert.deepEqual(s.normalize({ romances: [{ favorites: 'x', petPeeves: {}, unlocks: 'y' }] }).romances[0].unlocks, []);
assert.deepEqual(s.normalize({ romances: 'x', preferenceOptions: {} }).romances, []);

assert.deepEqual(s.createView(), { selected: null, selectedIndex: 0, pane: 'dossier', takeover: null, modal: null, popup: null, mode: 'hidden', lastRevision: -1 });

console.log('State: ok');
