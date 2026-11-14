import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const s = require('../../view/js/state.js');
const f = require('../../view/js/format.js');
const m = require('../../view/js/modals.js');

const sandbox = { window: {} };
vm.runInNewContext(readFileSync(new URL('../../view/dev/fixture.js', import.meta.url), 'utf8'), sandbox);
const state = s.normalize(sandbox.window.ROMANTASY_FIXTURE);
const row = state.romances.find(r => r.removable);
const has = (html, text) => assert.ok(html.includes(f.esc(text)), `missing ${JSON.stringify(text)}`);

const reset = m.modalHtml({ kind: 'reset', identity: s.rowIdentity(row) }, row);
for (const text of ['Player-created bond', 'Reset this bond?', "The personality remains. This companion's points and recent history reset in the current save only.", 'Reset bond', 'Cancel']) has(reset, text);
has(reset, row.name);
assert.match(reset, /data-action="modal\.confirm"/); assert.match(reset, /data-action="modal\.cancel"/);

const remove = m.modalHtml({ kind: 'remove', identity: s.rowIdentity(row) }, row);
for (const text of ['Player-created bond', 'Remove this bond?', 'This removes the bond from all saves using this configuration. Personalities supplied by follower mods remain protected.', 'Remove bond', 'Cancel']) has(remove, text);
has(remove, row.name);
assert.doesNotMatch(remove, /Reset this bond/);

const ERROR = 'The seal does not recognize that password.';
const password = m.modalHtml({ kind: 'password' });
for (const text of ['Restricted', 'DEVELOPER TOOLS', 'These utilities can rewrite every bond. Enter the password to unlock them.', 'Unlock', 'Cancel']) has(password, text);
assert.match(password, /<input type="password"/);
assert.ok(!password.includes(ERROR));
assert.match(password, /data-action="modal\.unlock"/);
has(m.modalHtml({ kind: 'password', error: true }), ERROR);

assert.equal(m.modalHtml(null), '');
assert.equal(m.modalHtml(undefined), '');
assert.equal(m.CLOSE_MS, 220);

// A row whose name carries markup and quotes reaches the sheet escaped and well formed.
const X = '"><img src=x onerror=1>\'';
const evil = s.normalize({ romances: [{ name: 'N' + X, referenceFormID: 'I' + X, points: 10, removable: true, personalityEditable: true, profileOrigin: 'player' }] }).romances[0];
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
for (const kind of ['reset', 'remove']) {
    const out = m.modalHtml({ kind, identity: s.rowIdentity(evil) }, evil);
    wellFormed(out, kind);
    assert.ok(out.includes('>N' + ESCAPED + '<'), `${kind} shows the name escaped`);
}
wellFormed(m.modalHtml({ kind: 'password', error: true }), 'password');

console.log('Modals: ok');
