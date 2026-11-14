import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const t = createRequire(import.meta.url)('../../view/js/tiers.js');

assert.deepEqual([-5, 0, 499, 500, 1240, 2499, 2500, 9999].map(t.tierIndex), [0, 0, 0, 1, 2, 4, 5, 5]);
assert.deepEqual([0, 1240, 2240, 2500].map(t.nextThreshold), [500, 1500, 2500, -1]);
assert.deepEqual([2240, 0, 2630].map(t.untilNext), [260, 500, 0]);
assert.deepEqual([0, 1250, 2500, 4000, -1].map(t.meterFraction), [0, 0.5, 1, 1, 0]);
assert.deepEqual([0, 250, 1240, 2500, 2630].map(t.tierProgress), [0, 0.5, 0.48, 1, 1]);
assert.deepEqual(t.TIERS.map(x => x.name), ['Stranger', 'Acquaintance', 'Friend', 'Confidant', 'Lover', 'Spouse']);
assert.deepEqual(t.TIERS.map(x => x.key), ['stranger', 'acquaintance', 'friend', 'confidant', 'lover', 'spouse']);
assert.deepEqual(t.TIERS.map(x => x.at), [0, 500, 1000, 1500, 2000, 2500]);
assert.equal(t.LADDER, 2500);
assert.equal(t.tier(1240).name, 'Friend');
const rows = [{ isFollowing: true }, { isFollowing: true }, { isFollowing: false }];
assert.deepEqual(t.countBonds(rows, true), { bonds: 3, following: 2 });
assert.deepEqual(t.countBonds(rows, false), { bonds: 2, following: 2 });

console.log('Tiers: ok');
