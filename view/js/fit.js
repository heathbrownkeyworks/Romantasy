// The stage fit: the design stage zooms to the window, clamped to 0.5..3.
// Dual-use: the maths is exported to Node for tests; the browser half applies it
// to #app with CSS zoom and sizes the stage to what the window has left, keeping a
// game-screen shape: never taller than 16:10 of its width (1600 wide: 1000 tall at most).
// A taller window centres the stage; the space above and below stays empty (the game shows there).
// Ledger size picks the stage: Standard 1600x900, Large 1422x800, Largest 1280x720. A smaller stage
// fitted to the same window draws everything bigger. A size it does not know is Standard.
(function () {
    'use strict';
    const STAGE_W = 1600, STAGE_H = 900, MAX_TALL = 0.625;
    const SIZES = { standard: { w: 1600, h: 900 }, large: { w: 1422, h: 800 }, largest: { w: 1280, h: 720 } };
    const known = size => typeof size === 'string' && Object.prototype.hasOwnProperty.call(SIZES, size);
    const stageOf = size => SIZES[known(size) ? size : 'standard'];
    let current = 'standard';

    function zoomFor(w, h, size) {
        const s = stageOf(size);
        return Math.min(Math.max(Math.min(w / s.w, h / s.h), 0.5), 3);
    }

    // stageFor(size): the stage of a ledger size, { w, h }.
    // stageFor(w, h, size): the stage in stage pixels at its zoom, and its top offset in window pixels.
    function stageFor(w, h, size) {
        if (h === undefined) { const s = stageOf(w); return { w: s.w, h: s.h }; }
        const zoom = zoomFor(w, h, size), width = w / zoom;
        const height = Math.min(h / zoom, width * MAX_TALL);
        return { zoom, width, height, top: Math.max(0, (h - height * zoom) / 2) };
    }

    // apply(size) changes the ledger size and fits again; apply() fits at the size it has.
    function apply(size) {
        if (size !== undefined) current = known(size) ? size : 'standard';
        if (typeof document === 'undefined') return;
        const app = document.getElementById('app');
        if (!app) return;
        const s = stageFor(window.innerWidth || STAGE_W, window.innerHeight || STAGE_H, current);
        app.style.zoom = s.zoom;
        app.style.width = s.width + 'px';
        app.style.height = s.height + 'px';
        // On the body, outside the zoom, so the offset is in window pixels on every engine.
        if (document.body) document.body.style.paddingTop = s.top + 'px';
    }

    const api = { STAGE_W, STAGE_H, SIZES, zoomFor, stageFor, apply };
    if (typeof module === 'object' && module.exports) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.fit = api;
    window.addEventListener('resize', () => apply());
    window.addEventListener('DOMContentLoaded', () => apply());
})();
