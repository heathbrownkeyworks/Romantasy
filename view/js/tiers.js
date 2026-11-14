// Romance tiers and the maths the dashboard draws from them. Thresholds match
// RomanceManager's romanceLevels (tests/ui/TiersTests.cpp holds the same values).
// A tier's name is the page's word for it (strings.js tier.1 to tier.6).
(function () {
    'use strict';
    const S = (typeof module === 'object' && module.exports) ? require('./strings.js') : window.RM.strings;
    const TIERS = [
        { key: 'stranger', name: S.t('tier.1'), at: 0 },
        { key: 'acquaintance', name: S.t('tier.2'), at: 500 },
        { key: 'friend', name: S.t('tier.3'), at: 1000 },
        { key: 'confidant', name: S.t('tier.4'), at: 1500 },
        { key: 'lover', name: S.t('tier.5'), at: 2000 },
        { key: 'spouse', name: S.t('tier.6'), at: 2500 }
    ];
    // The names the plugin sends for each level (RomanceManager's level names), in level order: matched, never shown.
    const SENT = ['Stranger', 'Acquaintance', 'Friend', 'Confidant', 'Lover', 'Spouse'];
    // The level (1 to 6) of a name the plugin sent, or 0 for any other text.
    const levelOf = name => SENT.indexOf(name) + 1;
    // A tier name the plugin sent, in the page's words: tier.N for level N's own name, else the plugin's text as it came.
    const shown = name => levelOf(name) ? S.or('tier.' + levelOf(name), name) : name;
    const LADDER = 2500, BAND = 500;

    function tierIndex(p) {
        let index = 0;
        for (let i = 0; i < TIERS.length; i++) if (p >= TIERS[i].at) index = i;
        return index;
    }
    const tier = p => TIERS[tierIndex(p)];
    function nextThreshold(p) {
        const i = tierIndex(p);
        return i >= TIERS.length - 1 ? -1 : TIERS[i + 1].at;
    }
    function untilNext(p) {
        const next = nextThreshold(p);
        return next < 0 ? 0 : next - p;
    }
    const meterFraction = p => Math.min(1, Math.max(0, p / LADDER));
    function tierProgress(p) {
        if (p >= LADDER) return 1;
        return Math.min(1, Math.max(0, (p - tier(p).at) / BAND));
    }
    function countBonds(rows, includeAway) {
        let bonds = 0, following = 0;
        for (const row of rows) {
            if (row.isFollowing) following++;
            if (includeAway || row.isFollowing) bonds++;
        }
        return { bonds, following };
    }

    const api = { TIERS, LADDER, tierIndex, tier, nextThreshold, untilNext, meterFraction, tierProgress, countBonds, levelOf, shown };
    if (typeof module === 'object' && module.exports) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.tiers = api;
})();
