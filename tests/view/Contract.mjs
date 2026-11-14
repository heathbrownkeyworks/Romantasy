import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';

const bridge = createRequire(import.meta.url)('../../view/js/bridge.js');
const read = rel => readFileSync(new URL('../../' + rel, import.meta.url), 'utf8');
const header = read('src/ui/magelight/Channels.h');
const parser = read('src/ui/PageRequests.cpp');

const strings = body => [...body.matchAll(/"([^"]*)"/g)].map(m => m[1]);
const scalar = name => {
    const m = header.match(new RegExp('\\b' + name + '\\s*=\\s*"([^"]*)"'));
    assert.ok(m, 'Channels.h declares ' + name);
    return m[1];
};
const array = name => {
    const m = header.match(new RegExp('\\b' + name + '\\s*\\{([^}]*)\\}'));
    assert.ok(m, 'Channels.h declares ' + name);
    return strings(m[1]);
};

assert.deepEqual([scalar('kState'), scalar('kMode'), scalar('kLevelChange'), scalar('kJournal')], bridge.TOPICS);
assert.deepEqual(array('kPageActions'), bridge.ACTIONS);
assert.deepEqual(array('kSettingsKeys'), bridge.PAYLOAD_KEYS.romantasySettings);
assert.deepEqual(array('kBondKeys'), bridge.PAYLOAD_KEYS.romantasyBond);
assert.deepEqual(array('kDevUnlockKeys'), bridge.PAYLOAD_KEYS.romantasyDevUnlock);
assert.deepEqual(array('kDebugKeys'), bridge.PAYLOAD_KEYS.romantasyDebug);
assert.deepEqual(array('kPopupAckKeys'), bridge.PAYLOAD_KEYS.romantasyPopupShown);
assert.deepEqual(array('kPopupAckKeys'), bridge.PAYLOAD_KEYS.romantasyPopupDismissed);
assert.deepEqual(array('kJournalKeys'), bridge.PAYLOAD_KEYS.romantasyJournalOpen);

const arrayName = {
    romantasySettings: 'kSettingsKeys', romantasyBond: 'kBondKeys',
    romantasyDevUnlock: 'kDevUnlockKeys', romantasyDebug: 'kDebugKeys',
    romantasyPopupShown: 'kPopupAckKeys', romantasyPopupDismissed: 'kPopupAckKeys',
    romantasyJournalOpen: 'kJournalKeys'
};
for (const [action, keys] of Object.entries(bridge.PAYLOAD_KEYS)) {
    for (const key of keys) {
        assert.ok(parser.includes('"' + key + '"') || parser.includes('channels::' + arrayName[action]),
            'PageRequests.cpp handles ' + action + '.' + key);
    }
}

console.log('Contract: ok');
