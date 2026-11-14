// Number and text formatting for the page. Digits are grouped by hand so the
// output does not depend on the host's locale support.
(function () {
    'use strict';
    // The page's words: when strings.js did not load or could not be read (a translated copy with a mistake, such as an
    // apostrophe inside single quotes), a stand-in shows every word as [key], so the page still opens, closes and answers
    // the plugin. This file loads right after strings.js on both pages.
    if (!(typeof module === 'object' && module.exports) && !(window.RM && window.RM.strings)) {
        window.RM = window.RM || {};
        window.RM.strings = { t: key => '[' + String(key) + ']', or: (key, fallback) => fallback, table: {} };
        if (typeof console === 'object' && console && typeof console.error === 'function') console.error('Romantasy: strings.js did not load or could not be read; every word shows as [key].');
    }
    function points(n) {
        const v = Math.round(Number(n) || 0);
        const digits = String(Math.abs(v));
        let out = '';
        for (let i = 0; i < digits.length; i++) {
            if (i > 0 && (digits.length - i) % 3 === 0) out += ',';
            out += digits[i];
        }
        return (v < 0 ? '-' : '') + out;
    }
    function signed(n) {
        const v = Math.round(Number(n) || 0);
        if (v > 0) return '+' + points(v);
        if (v < 0) return '−' + points(-v);
        return '0';
    }
    // Below Spouse a bond reads "2,240 / 2,500"; at or past the top it is just the points.
    function pointsWithThreshold(n) {
        const tiers = (typeof module === 'object' && module.exports) ? require('./tiers.js') : window.RM.tiers;
        const next = tiers.nextThreshold(n);
        return next < 0 ? points(n) : points(n) + ' / ' + points(next);
    }
    // Looked up when called: the whisper page loads this file for esc() alone.
    function deedCount(n) {
        const S = (typeof module === 'object' && module.exports) ? require('./strings.js') : window.RM.strings;
        return S.t(n === 1 ? 'enroll.deeds.one' : 'enroll.deeds.many', { count: n });
    }
    const ENTITIES = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };
    function esc(s) {
        return s === null || s === undefined ? '' : String(s).replace(/[&<>"']/g, c => ENTITIES[c]);
    }

    const api = { points, signed, pointsWithThreshold, deedCount, esc };
    if (typeof module === 'object' && module.exports) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.format = api;
})();
