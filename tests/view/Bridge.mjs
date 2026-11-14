import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const bridgePath = '../../view/js/bridge.js';
const bridge = createRequire(import.meta.url)(bridgePath);

assert.deepEqual(bridge.TOPICS, ['romantasyState', 'romantasyMode', 'romantasyLevelChange', 'romantasyJournal']);
assert.deepEqual(bridge.ACTIONS, ['romantasyReady', 'romantasyOpen', 'romantasyClose', 'romantasyRefresh', 'romantasySettings',
    'romantasyBond', 'romantasyDevUnlock', 'romantasyDevLock', 'romantasyDebug', 'romantasyPopupShown', 'romantasyPopupDismissed',
    'romantasyJournalOpen']);
assert.deepEqual(bridge.PAYLOAD_KEYS, {
    romantasySettings: ['openWithFavorites', 'showGainModals', 'showLossModals', 'showAwayFollowers', 'detailedLog', 'pace', 'tierMoment', 'whispers', 'whisperCorner', 'rosterOrder', 'ledgerSize', 'highContrast', 'reduceMotion', 'skyrimNetBridge', 'skyrimNetConversation', 'ostimMoments', 'ostimHold'],
    romantasyBond: ['kind', 'identity', 'preferences'],
    romantasyDevUnlock: ['password'],
    romantasyDebug: ['points', 'stat', 'delta'],
    romantasyPopupShown: ['seq'],
    romantasyPopupDismissed: ['seq'],
    romantasyJournalOpen: ['identity']
});
assert.equal(bridge.parse('not json'), null);
assert.deepEqual(bridge.parse('{"a":1}'), { a: 1 });

const store = bridge.createStore();
const seen = [];
store.publish('x', 1);
store.on('x', v => seen.push(v));
store.publish('x', 2);
assert.deepEqual(seen, [1, 2], 'a late subscriber gets the latest value, then each publish');
assert.equal(store.has('x'), true); assert.equal(store.has('y'), false);
assert.equal(store.get('x'), 2);

// The in-page half needs a window: run the file against a fake one holding a fake host.
const handlers = new Map(), sent = [], sounds = [];
const fake = { on: (name, fn) => handlers.set(name, fn), send: (name, text) => sent.push([name, text]), sound: name => sounds.push(name) };
const sandbox = { window: { magelight: fake }, console };
vm.runInNewContext(readFileSync(new URL(bridgePath, import.meta.url), 'utf8'), sandbox);
const b = sandbox.window.RM.bridge;

for (const topic of bridge.TOPICS) assert.ok(handlers.has(topic), 'attach subscribes ' + topic);
assert.ok(handlers.has('__uimode'), 'attach subscribes __uimode');
handlers.get('romantasyState')('{"revision":3}');
assert.equal(b.get('romantasyState').revision, 3);

const modes = [];
b.on('__uimode', v => modes.push(v));
handlers.get('__uimode')('1');
assert.equal(b.focused, true);
handlers.get('__uimode')('0');
assert.equal(b.focused, false);
assert.deepEqual(modes, [true, false]);

b.send('romantasySettings', { showGainModals: false });
assert.deepEqual(sent.at(-1), ['romantasySettings', '{"showGainModals":false}']);
b.send('romantasyReady');
assert.deepEqual(sent.at(-1), ['romantasyReady', '{}']);
assert.throws(() => b.send('nope'));
b.sound('chime');
assert.deepEqual(sounds, ['chime']);

console.log('Bridge: ok');
