import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const n = createRequire(import.meta.url)('../../view/js/notice.js');

assert.equal(n.isWarning('That companion could not be added.'), true);
assert.equal(n.isWarning('That bond cannot be reset here.'), true);
assert.equal(n.isWarning('Personalities are PROTECTED'), true);
assert.equal(n.isWarning('A new bond was entered in the ledger.'), false);
assert.equal(n.isWarning(undefined), false);
assert.equal(n.HOLD_MS, 3400);
assert.equal(n.shouldShow({ revision: 2, message: 'x' }, { lastRevision: 1 }), true);
assert.equal(n.shouldShow({ revision: 2, message: 'x' }, { lastRevision: 2 }), false);
assert.equal(n.shouldShow({ revision: 2, message: '' }, { lastRevision: 1 }), false);

console.log('Notice: ok');
