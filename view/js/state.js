// The page's state model: normalises the host's JSON and holds the pure rules
// for row identity, away-row filtering and selection.
(function () {
    'use strict';
    const tiers = (typeof module === 'object' && module.exports) ? require('./tiers.js') : window.RM.tiers;
    const S = (typeof module === 'object' && module.exports) ? require('./strings.js') : window.RM.strings;

    const asList = v => Array.isArray(v) ? v : [];
    const asText = v => typeof v === 'string' ? v : '';
    const isObject = v => v !== null && typeof v === 'object' && !Array.isArray(v);
    const asTexts = v => asList(v).filter(x => typeof x === 'string');
    // Unlock lines by tier: only levels 2 (Acquaintance) to 6 (Spouse), only string lines.
    const asUnlocks = v => asList(v).filter(u => isObject(u) && Number.isInteger(u.level) && u.level >= 2 && u.level <= 6)
        .map(u => ({ level: u.level, items: asTexts(u.items) }));

    // The bond's mood as the plugin names it, or '' (an older plugin sends none). It becomes a class name, so only these three pass.
    const MOODS = ['strained', 'settled', 'warm'];

    // The Slow burn gate ahead of the bond, or null: only levels 3 (Friend) to 6 (Spouse), only finite day counts.
    // The plugin sends it only under Slow burn while a gate is ahead.
    function asGate(g) {
        if (!isObject(g) || !Number.isInteger(g.level) || g.level < 3 || g.level > 6) return null;
        if (typeof g.daysNeeded !== 'number' || !isFinite(g.daysNeeded) || typeof g.daysTogether !== 'number' || !isFinite(g.daysTogether)) return null;
        if (g.daysNeeded < 1 || g.daysTogether < 0) return null;
        const name = typeof g.name === 'string' && g.name !== '' ? g.name : tiers.TIERS[g.level - 1].name;
        return { level: g.level, name, daysNeeded: g.daysNeeded, daysTogether: g.daysTogether };
    }

    function normalizeRow(r) {
        const points = typeof r.points === 'number' && isFinite(r.points) ? r.points : 0;
        return {
            name: asText(r.name),
            role: asText(r.role),
            points,
            levelName: typeof r.levelName === 'string' && r.levelName !== '' ? r.levelName : tiers.tier(points).name,
            isFollowing: r.isFollowing === true,
            mood: MOODS.includes(r.mood) ? r.mood : '',
            likes: asList(r.likes),
            dislikes: asList(r.dislikes),
            favorites: asTexts(r.favorites),
            petPeeves: asTexts(r.petPeeves),
            unlocks: asUnlocks(r.unlocks),
            gate: asGate(r.gate),
            // Each change's game day (a later plugin sends it): a finite number, else null.
            recent: asList(r.recent).map(c => isObject(c) ? Object.assign({}, c, { day: typeof c.day === 'number' && isFinite(c.day) ? c.day : null }) : c),
            profileOrigin: asText(r.profileOrigin),
            sourcePlugin: asText(r.sourcePlugin),
            profileFile: asText(r.profileFile),
            personalityEditable: r.personalityEditable === true,
            removable: r.removable === true,
            baseFormID: asText(r.baseFormID),
            referenceFormID: asText(r.referenceFormID)
        };
    }

    // The health report (null when the host sent none). Items keep the host's order: worst first.
    const LEVELS = ['note', 'warn', 'error'];
    function normalizeHealth(h) {
        if (!isObject(h)) return null;
        return {
            status: ['ok', 'warn', 'error'].includes(h.status) ? h.status : 'ok',
            version: asText(h.version),
            host: asText(h.host),
            bonds: Number.isInteger(h.bonds) && h.bonds > 0 ? h.bonds : 0,
            items: asList(h.items).filter(isObject).map(i => ({
                id: asText(i.id), level: LEVELS.includes(i.level) ? i.level : 'note', title: asText(i.title), detail: asText(i.detail)
            }))
        };
    }

    // Whether the control that opens Settings carries its mark (and Health shows first in Settings).
    const healthAlert = state => !!state.health && (state.health.status === 'warn' || state.health.status === 'error');
    // The word that goes with a level's mark; a note has none.
    const HEALTH_WORDS = { error: S.t('health.word.error'), warn: S.t('health.word.warn') };

    const PACES = ['slow', 'standard', 'fast'];
    const TIER_MOMENTS = ['calm', 'immediate'];
    const WHISPER_CORNERS = ['topLeft', 'topRight', 'bottomLeft', 'bottomRight'];
    const ROSTER_ORDERS = ['closest', 'name', 'recent'];
    const LEDGER_SIZES = ['standard', 'large', 'largest'];
    const choice = (value, allowed, dflt) => allowed.indexOf(value) >= 0 ? value : dflt;

    // SkyrimNet: found only when the plugin says exactly true; each switch on unless it says exactly false.
    function normalizeSkyrimNet(raw) {
        const n = isObject(raw) ? raw : {};
        return { found: n.found === true, bridge: n.bridge !== false, conversation: n.conversation !== false };
    }

    // OStim: found only when the plugin says exactly true; each switch on unless it says exactly false.
    function normalizeOStim(raw) {
        const n = isObject(raw) ? raw : {};
        return { found: n.found === true, moments: n.moments !== false, hold: n.hold !== false };
    }

    function normalize(raw) {
        const r = isObject(raw) ? raw : {};
        const flag = (key, dflt) => typeof r[key] === 'boolean' ? r[key] : dflt;
        return {
            message: asText(r.message),
            revision: typeof r.revision === 'number' && isFinite(r.revision) ? r.revision : 0,
            developerToolsUnlocked: flag('developerToolsUnlocked', false),
            openWithFavorites: flag('openWithFavorites', false),
            showGainModals: flag('showGainModals', true),
            showLossModals: flag('showLossModals', true),
            showAwayFollowers: flag('showAwayFollowers', true),
            detailedLog: flag('detailedLog', false),
            pace: choice(r.pace, PACES, 'standard'),
            tierMoment: choice(r.tierMoment, TIER_MOMENTS, 'calm'),
            whispers: flag('whispers', true),
            whisperCorner: choice(r.whisperCorner, WHISPER_CORNERS, 'topRight'),
            rosterOrder: choice(r.rosterOrder, ROSTER_ORDERS, 'closest'),
            ledgerSize: choice(r.ledgerSize, LEDGER_SIZES, 'standard'),
            highContrast: flag('highContrast', false),
            reduceMotion: flag('reduceMotion', false),
            skyrimNet: normalizeSkyrimNet(r.skyrimNet),
            ostim: normalizeOStim(r.ostim),
            health: normalizeHealth(r.health),
            romances: asList(r.romances).filter(isObject).map(normalizeRow),
            enrollmentCandidates: asList(r.enrollmentCandidates),
            preferenceOptions: asList(r.preferenceOptions)
        };
    }

    function rowIdentity(row) {
        const ref = row.referenceFormID;
        return ref && ref !== '00000000' ? ref : row.baseFormID;
    }

    const visibleRows = state => state.showAwayFollowers === false ? state.romances.filter(r => r.isFollowing) : state.romances;

    function reconcileSelection(previousIdentity, previousIndex, rows) {
        if (rows.length === 0) return null;
        if (previousIdentity !== null && previousIdentity !== undefined) {
            for (const row of rows) if (rowIdentity(row) === previousIdentity) return previousIdentity;
        }
        return rowIdentity(rows[Math.max(0, Math.min(previousIndex, rows.length - 1))]);
    }

    const createView = () => ({ selected: null, selectedIndex: 0, pane: 'dossier', takeover: null, modal: null, popup: null, mode: 'hidden', lastRevision: -1 });

    const api = { normalize, healthAlert, HEALTH_WORDS, rowIdentity, visibleRows, reconcileSelection, createView };
    if (typeof module === 'object' && module.exports) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.state = api;
})();
