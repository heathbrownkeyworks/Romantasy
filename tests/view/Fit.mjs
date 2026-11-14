import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const fit = createRequire(import.meta.url)('../../view/js/fit.js');

assert.equal(fit.STAGE_W, 1600); assert.equal(fit.STAGE_H, 900);
assert.equal(fit.zoomFor(1600, 900), 1);
assert.equal(fit.zoomFor(3840, 2160), 2.4);
assert.equal(fit.zoomFor(1920, 1200), 1.2, 'the smaller axis wins');
assert.equal(fit.zoomFor(640, 360), 0.5, 'the fit clamps at 0.5');
assert.equal(fit.zoomFor(8000, 4500), 3, 'the fit clamps at 3');

// The stage keeps a game-screen shape: never taller than 16:10 of its width. A taller window centres it,
// the space above and below left empty (the game shows there); a wider one widens it as before.
const near = (actual, expected, what) => assert.ok(Math.abs(actual - expected) < 1e-6, what + ': ' + actual + ' is not ' + expected);
function stage(w, h, zoom, width, height, top) {
    const s = fit.stageFor(w, h);
    near(s.zoom, zoom, w + 'x' + h + ' zoom');
    near(s.width, width, w + 'x' + h + ' width');
    near(s.height, height, w + 'x' + h + ' height');
    near(s.top, top, w + 'x' + h + ' top');
}
stage(1920, 1080, 1.2, 1600, 900, 0);
stage(2560, 1440, 1.6, 1600, 900, 0);
stage(3440, 1440, 1.6, 2150, 900, 0);                         // ultrawide: wider, as today
stage(1600, 900, 1, 1600, 900, 0);
stage(1920, 1200, 1.2, 1600, 1000, 0);                        // 16:10 exactly fills
stage(1600, 1200, 1, 1600, 1000, (1200 - 1000) / 2);          // 4:3: centred
stage(1070, 1206, 1070 / 1600, 1600, 1000, (1206 - 1000 * 1070 / 1600) / 2);   // the preview pane
stage(640, 360, 0.5, 1280, 720, 0);                           // the 0.5 clamp, as today
stage(8000, 4500, 3, 8000 / 3, 1500, 0);                      // the 3 clamp, as today
for (const [w, h] of [[1070, 1206], [1600, 1200], [1024, 1280], [800, 2000]]) {
    const s = fit.stageFor(w, h);
    assert.ok(s.height <= s.width * 0.625 + 1e-9, w + 'x' + h + ' stays within 16:10');
    near(s.top * 2 + s.height * s.zoom, h, w + 'x' + h + ' is centred');
}

// Ledger size: the layout stage shrinks (1422x800, 1280x720) so the same window draws everything bigger.
// stageFor(size) gives the stage; an unknown size is the standard one. The two-argument calls above mean standard.
assert.deepEqual(fit.stageFor('standard'), { w: 1600, h: 900 });
assert.deepEqual(fit.stageFor('large'), { w: 1422, h: 800 });
assert.deepEqual(fit.stageFor('largest'), { w: 1280, h: 720 });
for (const size of [undefined, null, '', 'huge', 'LARGE', 3, {}]) assert.deepEqual(fit.stageFor(size), { w: 1600, h: 900 }, 'an unknown size is standard: ' + String(size));
assert.equal(fit.zoomFor(1920, 1080, 'standard'), 1.2);
assert.equal(fit.zoomFor(1920, 1080, 'large'), 1.35);
assert.equal(fit.zoomFor(1920, 1080, 'largest'), 1.5);
assert.equal(fit.zoomFor(1920, 1080, 'nonsense'), 1.2, 'an unknown size zooms as standard');
assert.equal(fit.zoomFor(1920, 1080), fit.zoomFor(1920, 1080, 'standard'), 'two arguments mean standard');
assert.equal(fit.zoomFor(320, 180, 'largest'), 0.5, 'still clamped at 0.5');
assert.equal(fit.zoomFor(8000, 4500, 'largest'), 3, 'still clamped at 3');
// The stage at each size keeps the same game-screen shape: the same 16:10 clamp, centred the same way.
function sized(size, w, h, zoom, width, height, top) {
    const s = fit.stageFor(w, h, size);
    near(s.zoom, zoom, size + ' ' + w + 'x' + h + ' zoom');
    near(s.width, width, size + ' ' + w + 'x' + h + ' width');
    near(s.height, height, size + ' ' + w + 'x' + h + ' height');
    near(s.top, top, size + ' ' + w + 'x' + h + ' top');
}
sized('large', 1920, 1080, 1.35, 1920 / 1.35, 800, 0);
sized('largest', 1920, 1080, 1.5, 1280, 720, 0);
sized('largest', 1280, 720, 1, 1280, 720, 0);
sized('largest', 1600, 900, 1.25, 1280, 720, 0);
sized('largest', 1680, 1050, 1680 / 1280, 1280, 800, 0);                 // 16:10 exactly fills
sized('large', 1600, 1200, 1600 / 1422, 1422, 1422 * 0.625, (1200 - 1422 * 0.625 * 1600 / 1422) / 2);   // 4:3: centred
sized('largest', 3440, 1440, 2, 1720, 720, 0);                            // ultrawide: wider
for (const size of ['large', 'largest']) {
    for (const [w, h] of [[1070, 1206], [1600, 1200], [1024, 1280], [800, 2000], [1680, 1050]]) {
        const s = fit.stageFor(w, h, size);
        assert.ok(s.height <= s.width * 0.625 + 1e-9, size + ' ' + w + 'x' + h + ' stays within 16:10');
        near(s.top * 2 + s.height * s.zoom, h, size + ' ' + w + 'x' + h + ' is centred');
    }
}
// The standard stage is exactly what it was: the two-argument call and the 'standard' call agree everywhere.
for (const [w, h] of [[1920, 1080], [1070, 1206], [3440, 1440], [640, 360], [1680, 1050]]) {
    assert.deepEqual(fit.stageFor(w, h, 'standard'), fit.stageFor(w, h));
}

console.log('Fit: ok');
