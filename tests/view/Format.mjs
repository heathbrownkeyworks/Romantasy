import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';

const formatPath = new URL('../../view/js/format.js', import.meta.url);
const f = createRequire(import.meta.url)('../../view/js/format.js');

assert.equal(f.points(0), '0'); assert.equal(f.points(999), '999');
assert.equal(f.points(1240), '1,240'); assert.equal(f.points(1234567), '1,234,567'); assert.equal(f.points(-1500), '-1,500');
assert.equal(f.signed(12), '+12'); assert.equal(f.signed(-30), '−30'); assert.equal(f.signed(0), '0');
assert.equal(f.pointsWithThreshold(2240), '2,240 / 2,500'); assert.equal(f.pointsWithThreshold(2630), '2,630');
assert.equal(f.deedCount(1), '1 deed'); assert.equal(f.deedCount(0), '0 deeds'); assert.equal(f.deedCount(7), '7 deeds');
assert.equal(f.esc('<b>"A" & \'B\'</b>'), '&lt;b&gt;&quot;A&quot; &amp; &#39;B&#39;&lt;/b&gt;');
assert.equal(f.esc(null), ''); assert.equal(f.esc(42), '42'); assert.equal(f.esc('Серана'), 'Серана');
assert.doesNotMatch(readFileSync(formatPath, 'utf8'), /toLocaleString|Intl\./);

console.log('Format: ok');
