// The browser harness's stand-in host (view/dev/mock.js) logs every action, but never a password.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const lines = [];
const record = (...args) => lines.push(args.map(a => typeof a === 'string' ? a : JSON.stringify(a)).join(' '));
const element = () => ({ classList: { add() {} }, appendChild() {} });
const window = { ROMANTASY_FIXTURE: { romances: [] } };
const box = {
    window, location: { search: '?mock' },
    document: { documentElement: element(), head: element(), createElement: element },
    console: { log: record, warn: record, error: record }
};
box.window.window = window;
vm.runInNewContext(readFileSync(new URL('../../view/dev/mock.js', import.meta.url), 'utf8'), box);

window.magelight.send('romantasyDevUnlock', JSON.stringify({ password: 'hunter2-secret' }));
assert.ok(lines.some(l => l.includes('romantasyDevUnlock')), 'the unlock is still logged');
assert.ok(!lines.some(l => l.includes('hunter2-secret')), 'the password never reaches the console');
assert.equal(window.__romantasyMock.state.developerToolsUnlocked, undefined, 'a wrong password does not unlock');
window.magelight.send('romantasyDevUnlock', JSON.stringify({ password: 'mock' }));
assert.equal(window.__romantasyMock.state.developerToolsUnlocked, true, 'the mock password still unlocks');
assert.ok(!lines.some(l => /"password":"mock"/.test(l)), 'not even the mock password');

// &health=: plain ?mock is healthy; warn, error and note give items of those levels, worst first.
function mockState(search) {
    const w = { ROMANTASY_FIXTURE: { romances: [{ name: 'A' }, { name: 'B' }] } };
    const b = { window: w, location: { search }, document: { documentElement: element(), head: element(), createElement: element }, console: { log() {}, warn() {}, error() {} } };
    w.window = w;
    vm.runInNewContext(readFileSync(new URL('../../view/dev/mock.js', import.meta.url), 'utf8'), b);
    return w.__romantasyMock.state;
}
const levelsOf = st => [...st.health.items].map(i => i.level);
const healthy = mockState('?mock');
assert.equal(healthy.health.status, 'ok');
assert.equal(healthy.health.items.length, 0);
assert.equal(healthy.health.version, '3.0.1');
assert.equal(healthy.health.host, '1.6.2');
assert.equal(healthy.health.bonds, 2);
assert.equal(healthy.detailedLog, false);
assert.equal(healthy.pace, 'standard');
assert.equal(healthy.tierMoment, 'calm');
assert.equal(healthy.whispers, true);
assert.equal(healthy.whisperCorner, 'topRight');
assert.equal(healthy.rosterOrder, 'closest');
assert.equal(healthy.ledgerSize, 'standard');
assert.equal(healthy.highContrast, false);
assert.equal(healthy.reduceMotion, false);
// &skyrimnet: SkyrimNet was found, so Settings shows its two rows; plain ?mock has no SkyrimNet.
assert.deepEqual(JSON.parse(JSON.stringify(healthy.skyrimNet)), { found: false, bridge: true, conversation: true });
assert.deepEqual(JSON.parse(JSON.stringify(mockState('?mock&skyrimnet').skyrimNet)), { found: true, bridge: true, conversation: true });
assert.equal(mockState('?mock&skyrimnetx').skyrimNet.found, false, 'only the exact switch');
// &ostim: OStim was found, so Settings shows its two rows; plain ?mock has no OStim.
assert.deepEqual(JSON.parse(JSON.stringify(healthy.ostim)), { found: false, moments: true, hold: true });
assert.deepEqual(JSON.parse(JSON.stringify(mockState('?mock&ostim').ostim)), { found: true, moments: true, hold: true });
assert.equal(mockState('?mock&ostimx').ostim.found, false, 'only the exact switch');
const warned = mockState('?mock&health=warn');
assert.equal(warned.health.status, 'warn');
assert.deepEqual(levelsOf(warned), ['warn', 'note']);
assert.deepEqual([...warned.health.items].map(i => i.id), ['profiles', 'orphans']);
const failed = mockState('?mock&health=error');
assert.equal(failed.health.status, 'error');
assert.deepEqual(levelsOf(failed), ['error', 'warn', 'note']);
assert.deepEqual([...failed.health.items].map(i => i.id), ['conditions', 'power', 'orphans']);
const noted = mockState('?mock&health=note');
assert.equal(noted.health.status, 'ok');
assert.deepEqual(levelsOf(noted), ['note']);
for (const st of [warned, failed, noted]) for (const item of st.health.items) assert.ok(item.title && item.detail, `${item.id} has a title and a detail`);

// The Detailed log switch reaches the mock's state.
window.magelight.send('romantasySettings', JSON.stringify({ detailedLog: true }));
assert.equal(window.__romantasyMock.state.detailedLog, true);
window.magelight.send('romantasySettings', JSON.stringify({ pace: 'fast', tierMoment: 'immediate' }));
assert.equal(window.__romantasyMock.state.pace, 'fast');
assert.equal(window.__romantasyMock.state.tierMoment, 'immediate');
window.magelight.send('romantasySettings', JSON.stringify({ pace: 'turbo', tierMoment: 'later' }));
assert.equal(window.__romantasyMock.state.pace, 'fast');
assert.equal(window.__romantasyMock.state.tierMoment, 'immediate');
// The whisper settings reach it too; a bad value is ignored.
window.magelight.send('romantasySettings', JSON.stringify({ whispers: false, whisperCorner: 'bottomLeft' }));
assert.equal(window.__romantasyMock.state.whispers, false);
assert.equal(window.__romantasyMock.state.whisperCorner, 'bottomLeft');
window.magelight.send('romantasySettings', JSON.stringify({ rosterOrder: 'name', ledgerSize: 'large', highContrast: true, reduceMotion: true }));
assert.equal(window.__romantasyMock.state.rosterOrder, 'name');
assert.equal(window.__romantasyMock.state.ledgerSize, 'large');
assert.equal(window.__romantasyMock.state.highContrast, true);
assert.equal(window.__romantasyMock.state.reduceMotion, true);
window.magelight.send('romantasySettings', JSON.stringify({ rosterOrder: 'Name', ledgerSize: 'huge', highContrast: 'yes', reduceMotion: 1 }));
assert.equal(window.__romantasyMock.state.rosterOrder, 'name');
assert.equal(window.__romantasyMock.state.ledgerSize, 'large');
assert.equal(window.__romantasyMock.state.highContrast, true);
assert.equal(window.__romantasyMock.state.reduceMotion, true);
window.magelight.send('romantasySettings', JSON.stringify({ skyrimNetBridge: false, skyrimNetConversation: false }));
assert.equal(window.__romantasyMock.state.skyrimNet.bridge, false);
assert.equal(window.__romantasyMock.state.skyrimNet.conversation, false);
window.magelight.send('romantasySettings', JSON.stringify({ skyrimNetBridge: 'yes', skyrimNetConversation: 1 }));
assert.equal(window.__romantasyMock.state.skyrimNet.bridge, false);
assert.equal(window.__romantasyMock.state.skyrimNet.conversation, false);
window.magelight.send('romantasySettings', JSON.stringify({ skyrimNetBridge: true }));
assert.equal(window.__romantasyMock.state.skyrimNet.bridge, true);
window.magelight.send('romantasySettings', JSON.stringify({ ostimMoments: false, ostimHold: false }));
assert.equal(window.__romantasyMock.state.ostim.moments, false);
assert.equal(window.__romantasyMock.state.ostim.hold, false);
window.magelight.send('romantasySettings', JSON.stringify({ ostimMoments: 'yes', ostimHold: 1 }));
assert.equal(window.__romantasyMock.state.ostim.moments, false);
assert.equal(window.__romantasyMock.state.ostim.hold, false);
window.magelight.send('romantasySettings', JSON.stringify({ ostimHold: true }));
assert.equal(window.__romantasyMock.state.ostim.hold, true);
assert.equal(window.__romantasyMock.state.ostim.moments, false, 'the two are independent');
window.magelight.send('romantasySettings', JSON.stringify({ whispers: 'yes', whisperCorner: 'middle' }));
assert.equal(window.__romantasyMock.state.whispers, false);
assert.equal(window.__romantasyMock.state.whisperCorner, 'bottomLeft');

// The mock shows the richer personalities: a favorite, a pet peeve, unlock lines, and a gain popup carrying some.
function realMock(search) {
    const w = {};
    vm.runInNewContext(readFileSync(new URL('../../view/dev/fixture.js', import.meta.url), 'utf8'), { window: w });
    const b = { window: w, location: { search }, document: { documentElement: element(), head: element(), createElement: element }, console: { log() {}, warn() {}, error() {} } };
    w.window = w;
    vm.runInNewContext(readFileSync(new URL('../../view/dev/mock.js', import.meta.url), 'utf8'), b);
    return w;
}
const rich = realMock('?mock').__romantasyMock.state.romances;
const twoAndOne = rich.find(r => (r.favorites || []).length === 2 && (r.petPeeves || []).length === 1);
assert.ok(twoAndOne, 'one row has two favorites and one pet peeve');
for (const f of twoAndOne.favorites) assert.ok(twoAndOne.likes.includes(f), f + ' is also a like');
for (const p of twoAndOne.petPeeves) assert.ok(twoAndOne.dislikes.includes(p), p + ' is also a dislike');
const TIER_AT = [0, 500, 1000, 1500, 2000, 2500];
const levelOf = points => TIER_AT.filter(at => points >= at).length;
const unlocking = rich.find(r => r.profileOrigin === 'author' && Array.isArray(r.unlocks)
    && r.unlocks.filter(u => u.level > levelOf(r.points)).length >= 2 && r.unlocks.some(u => u.level <= levelOf(r.points)));
assert.ok(unlocking, 'an author row has unlock lines for two tiers above it and one at or below it');
assert.ok(unlocking.unlocks.find(u => u.level === levelOf(unlocking.points) + 1 && u.items.length), 'including the next tier');
// Every mood shows on some row, and one row has no mood field at all (as from an older plugin).
assert.deepEqual([...new Set(rich.filter(r => 'mood' in r).map(r => r.mood))].sort(), ['settled', 'strained', 'warm']);
assert.equal(rich.filter(r => !('mood' in r)).length, 1, 'one row without the field');
// &popup=gain: the level change that follows the first state carries two unlock lines.
const gw = realMock('?mock&popup=gain');
const changes = [];
gw.magelight.on('romantasyLevelChange', text => changes.push(JSON.parse(text)));
gw.magelight.send('romantasyReady', '{}');
assert.equal(changes.length, 1);
assert.equal(changes[0].changeDirection, 'gain');
assert.equal(changes[0].unlocks.length, 2);
assert.ok(changes[0].unlocks.every(u => typeof u === 'string' && u !== ''));
gw.__romantasyMock.popup('loss');
assert.deepEqual(changes[1].unlocks, [], 'a loss carries none');

// The health sample quotes the plugin's current message.
assert.ok(warned.health.items.find(i => i.id === 'profiles').detail.includes('schema_version must be integer 1, 2 or 3'));
assert.ok(!/integer 1(?!, 2 or 3)/.test(warned.health.items.find(i => i.id === 'profiles').detail), 'no line quotes the old message');

// &heavy: enough of everything that every scroller has to scroll.
const heavy = realMock('?mock&heavy').__romantasyMock.state;
const first = heavy.romances[0];
assert.equal(heavy.romances.length, 30, 'the roster has 30 bonds');
assert.equal(new Set(heavy.romances.map(r => r.referenceFormID)).size, 30, 'each bond its own identity');
assert.equal(first.likes.length, 30);
assert.equal(first.dislikes.length, 30);
assert.equal(first.favorites.length, 3);
assert.equal(first.petPeeves.length, 3);
for (const f of first.favorites) assert.ok(first.likes.includes(f));
for (const p of first.petPeeves) assert.ok(first.dislikes.includes(p));
assert.equal(first.recent.length, 40);
const next = first.unlocks.find(u => u.level === levelOf(first.points) + 1);
assert.ok(next && next.items.length === 4 && next.items.every(line => line.length >= 50 && line.length <= 60), 'four long unlock lines for the next tier');
assert.ok(first.sourcePlugin.length > 200, 'a long personality text');
assert.equal(realMock('?mock').__romantasyMock.state.romances.length, 8, 'plain ?mock is unchanged');

// &pace=slow: Slow burn, and the first bond has a gate ahead for its next tier (plain ?mock has none).
const slow = realMock('?mock&pace=slow');
const slowState = slow.__romantasyMock.state;
assert.equal(slowState.pace, 'slow');
const gatedRows = slowState.romances.filter(r => r.gate);
assert.equal(gatedRows.length, 1, 'one row has a gate');
const gate = gatedRows[0].gate;
assert.equal(gate.level, levelOf(gatedRows[0].points) + 1, 'the gate is for the next tier');
assert.equal(gate.name, ['Stranger', 'Acquaintance', 'Friend', 'Confidant', 'Lover', 'Spouse'][gate.level - 1]);
assert.equal(gate.daysNeeded, { 3: 3, 4: 7, 5: 14, 6: 21 }[gate.level]);
assert.ok(Number.isFinite(gate.daysTogether) && gate.daysTogether >= 0 && gate.daysTogether < gate.daysNeeded);
assert.ok(realMock('?mock&heavy&pace=slow').__romantasyMock.state.romances[0].gate, '&heavy&pace=slow: the heavy bond carries it');
assert.ok(!realMock('?mock').__romantasyMock.state.romances.some(r => 'gate' in r), 'plain ?mock: no gate');
// A choice row sends one key; the mock answers it as the plugin does, and the gate follows the pace.
const states = [];
slow.magelight.on('romantasyState', text => states.push(JSON.parse(text)));
slow.magelight.send('romantasySettings', JSON.stringify({ pace: 'fast' }));
assert.equal(states.length, 1, 'a choice is answered with a state');
assert.equal(states[0].pace, 'fast');
assert.equal(states[0].tierMoment, 'calm', 'the other choice is untouched');
assert.ok(!states[0].romances.some(r => r.gate), 'no gate away from Slow burn');
slow.magelight.send('romantasySettings', JSON.stringify({ tierMoment: 'immediate' }));
assert.equal(states[1].tierMoment, 'immediate');
assert.equal(states[1].pace, 'fast');
slow.magelight.send('romantasySettings', JSON.stringify({ pace: 'slow' }));
assert.equal(states[2].romances.filter(r => r.gate).length, 1, 'Slow burn again: the gate is back');

// The journal: the mock answers romantasyJournalOpen as the plugin does, carrying the identity it was asked for.
function journalOf(search, pick) {
    const w = realMock(search), answers = [];
    w.magelight.on('romantasyJournal', text => answers.push(JSON.parse(text)));
    const rowsOf = w.__romantasyMock.state.romances;
    const identity = pick ? pick(rowsOf) : rowsOf[0].referenceFormID;
    w.magelight.send('romantasyJournalOpen', JSON.stringify({ identity }));
    assert.equal(answers.length, 1, 'one answer per request');
    assert.equal(answers[0].identity, identity);
    return answers[0];
}
const KINDS = ['began', 'reached', 'fell'];
const journal = journalOf('?mock');
assert.equal(journal.found, true);
assert.ok(journal.milestones.length >= 3, 'at least 3 milestones');
assert.equal(journal.milestones[0].kind, 'began', 'the journal begins first');
for (const m of journal.milestones) assert.ok(KINDS.includes(m.kind) && Number.isInteger(m.level) && m.level >= 1 && m.level <= 6 && Number.isInteger(m.day) && m.day >= 1);
assert.ok(journal.milestones.some(m => m.kind === 'fell'), 'a fall');
assert.ok(journal.days.length >= 20, 'at least 20 days');
assert.ok(journal.days.some(d => d.gained > 0 && d.lost === 0) && journal.days.some(d => d.lost > 0 && d.gained === 0), 'days of only a gain and of only a loss');
for (let i = 1; i < journal.days.length; i++) assert.ok(journal.days[i].day > journal.days[i - 1].day, 'days oldest first');
assert.ok(journal.gains.length > 0 && journal.losses.length > 0 && journal.history.length > 0, 'moments and history');
assert.ok(journal.gains.every(c => c.points > 0) && journal.losses.every(c => c.points < 0));
assert.ok(journal.daysTogether > 0 && journal.began === journal.milestones[0].day);
// The last tier reached and fallen to leaves the bond at its tier now.
const firstRow = realMock('?mock').__romantasyMock.state.romances[0];
const lastTier = journal.milestones.filter(m => m.kind !== 'began').pop();
assert.ok(lastTier.level <= levelOf(firstRow.points), 'no milestone above the bond\'s tier');
// &heavy: the fullest journal the plugin keeps, with labels at the 120-byte limit.
const full = journalOf('?mock&heavy');
assert.deepEqual([full.milestones.length, full.days.length, full.gains.length, full.losses.length, full.history.length], [26, 60, 5, 5, 30]);
assert.ok(full.history.every(c => Buffer.byteLength(c.label) <= 120) && full.history.some(c => Buffer.byteLength(c.label) === 120), 'labels up to 120 bytes');
assert.equal(full.milestones.filter(m => m.kind === 'fell').length, 20, 'twenty falls');
// &ostim: the first bond's journal carries one "A first moment together", level 0, in day order; the others none.
assert.ok(!journal.milestones.some(m => m.kind === 'shared'), 'plain ?mock has no shared moment');
const withMoment = journalOf('?mock&ostim');
const sharedOnes = withMoment.milestones.filter(m => m.kind === 'shared');
assert.equal(sharedOnes.length, 1, 'one shared milestone');
assert.ok(sharedOnes[0].level === 0 && Number.isInteger(sharedOnes[0].day) && sharedOnes[0].day >= 1);
const at = withMoment.milestones.indexOf(sharedOnes[0]);
assert.ok(withMoment.milestones.slice(0, at).every(m => m.day <= sharedOnes[0].day) && withMoment.milestones.slice(at + 1).every(m => m.day > sharedOnes[0].day));
assert.deepEqual(withMoment.milestones.filter(m => m.kind !== 'shared'), journal.milestones, 'the rest unchanged');
assert.ok(!journalOf('?mock&ostim', rows => rows[1].referenceFormID).milestones.some(m => m.kind === 'shared'), 'only the first bond');
const heavyMoment = journalOf('?mock&heavy&ostim');
assert.equal(heavyMoment.milestones.length, 27, 'the fullest journal plus the shared one');
// Another identity, and &fail: found false.
assert.deepEqual(journalOf('?mock', () => 'FFFFFFFF'), { identity: 'FFFFFFFF', found: false });
assert.deepEqual(journalOf('?mock&fail'), { identity: realMock('?mock&fail').__romantasyMock.state.romances[0].referenceFormID, found: false });

console.log('Mock: ok');
