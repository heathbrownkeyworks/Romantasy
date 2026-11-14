// The Journal pane: one bond's journal in place of its dossier, with Back to the dossier.
// normalize(raw) and journalHtml(row, data) are pure and run under Node for the tests; render() below the
// export line paints #pane while view.pane is 'journal'.
//
// The page asks for the journal (romantasyJournalOpen with the bond's identity) when the pane opens; the plugin
// answers on the topic romantasyJournal, carrying the identity it was asked for. An answer for another identity
// is ignored. With no answer after 3 seconds the pane says it could not be opened; a later answer for the open
// bond still fills it.
//
// Actions: journal.open (data-target: the bond's identity), journal.back.
(function () {
    'use strict';
    const node = typeof module === 'object' && module.exports;
    const tiers = node ? require('./tiers.js') : window.RM.tiers;
    const format = node ? require('./format.js') : window.RM.format;
    const S = node ? require('./strings.js') : window.RM.strings;
    const esc = format.esc;
    const tx = (key, values) => esc(S.t(key, values));

    // The plugin's limits (src/romance/Journal.h): 30 history rows, 60 days, 5 moments each way, 27 milestones
    // (one "Journal began", five "Reached", twenty "Fell to", plus the "A first moment together" that
    // src/ui/JournalJson.cpp adds), a label of 120 bytes.
    const LIMITS = { history: 30, days: 60, moments: 5, milestones: 27, label: 120 };
    const KINDS = ['began', 'reached', 'fell', 'shared'];
    const WAIT_MS = 3000;

    const isObject = v => v !== null && typeof v === 'object' && !Array.isArray(v);
    const asList = v => Array.isArray(v) ? v : [];
    const finite = v => typeof v === 'number' && isFinite(v);
    // A "Day N" number: at least 1, whole.
    const dayOf = v => Math.max(1, Math.floor(v));
    // At most 120 characters: the plugin cuts at 120 bytes, so this only ever cuts what it would not have sent.
    const cut = v => typeof v === 'string' ? Array.from(v).slice(0, LIMITS.label).join('') : '';

    function change(c) {
        if (!isObject(c) || !finite(c.points) || !finite(c.day)) return null;
        return { label: cut(c.label), points: Math.round(c.points), day: dayOf(c.day) };
    }
    // keep: which points the list holds (a deepening moment gains, a wounding one loses); the rest are dropped before the cut.
    const changes = (v, max, keep) => asList(v).map(change).filter(c => c && (!keep || keep(c.points))).slice(0, max);

    // "shared" (the first moment together) carries level 0; the others a tier, 1 to 6.
    const levelFits = m => m.kind === 'shared' ? m.level === 0 : m.level >= 1 && m.level <= tiers.TIERS.length;
    function milestone(m) {
        if (!isObject(m) || KINDS.indexOf(m.kind) < 0 || !Number.isInteger(m.level) || !levelFits(m) || !finite(m.day)) return null;
        return { kind: m.kind, level: m.level, day: dayOf(m.day) };
    }

    function day(d) {
        if (!isObject(d) || !finite(d.day)) return null;
        const size = v => finite(v) && v > 0 ? Math.round(v) : 0;
        return { day: dayOf(d.day), gained: size(d.gained), lost: size(d.lost) };
    }

    // The plugin's answer as the page uses it, or null for anything that is not an object. Only the known fields,
    // each of its type: numbers finite, lists cut to the plugin's limits (the newest days kept), entries of the
    // wrong shape dropped, and so are moments of the wrong sign (a gain of 0 or less, a loss of 0 or more). Running it on its own output changes nothing.
    function normalize(raw) {
        if (!isObject(raw)) return null;
        const found = raw.found === true;
        return {
            identity: typeof raw.identity === 'string' ? raw.identity : '',
            found,
            began: found && finite(raw.began) && raw.began >= 1 ? Math.floor(raw.began) : 0,
            daysTogether: found && finite(raw.daysTogether) && raw.daysTogether > 0 ? raw.daysTogether : 0,
            milestones: found ? asList(raw.milestones).map(milestone).filter(Boolean).slice(0, LIMITS.milestones) : [],
            days: found ? asList(raw.days).map(day).filter(Boolean).slice(-LIMITS.days) : [],
            gains: found ? changes(raw.gains, LIMITS.moments, p => p > 0) : [],
            losses: found ? changes(raw.losses, LIMITS.moments, p => p < 0) : [],
            history: found ? changes(raw.history, LIMITS.history) : []
        };
    }

    const isEmpty = d => !d.milestones.length && !d.days.length && !d.gains.length && !d.losses.length && !d.history.length;
    const tierName = level => tiers.TIERS[level - 1].name;
    const MILESTONE = { began: () => S.t('journal.began'), reached: level => S.t('journal.reached', { tier: tierName(level) }), fell: level => S.t('journal.fell', { tier: tierName(level) }), shared: () => S.t('journal.shared') };

    // "12.5 days together", "1 day together".
    function together(n) {
        const v = Math.round(n * 10) / 10;
        return S.t(v === 1 ? 'journal.together.one' : 'journal.together.many', { days: v % 1 === 0 ? String(v) : v.toFixed(1) });
    }

    function milestonesHtml(list) {
        if (!list.length) return '';
        return '<section class="jcard glass"><h3 class="eyebrow">' + tx('journal.milestones') + '</h3><ol class="jmiles">'
            + list.map(m => '<li class="jmile ' + m.kind + '">' + tx('journal.milestone', { day: m.day, what: MILESTONE[m.kind](m.level) }) + '</li>').join('')
            + '</ol></section>';
    }

    // One bar per day: the gain above the hairline, the loss below it, each sized against the largest value of
    // either kind in the set (0 when every value is 0).
    function chartHtml(days) {
        if (!days.length) return '';
        const largest = days.reduce((max, d) => Math.max(max, d.gained, d.lost), 0);
        const pct = v => largest > 0 ? Math.round(v / largest * 1000) / 10 : 0;
        const bar = (cls, v) => '<span class="' + cls + (v > 0 ? ' some' : '') + '" style="height:' + pct(v) + '%"></span>';
        let gained = 0, lost = 0;
        const bars = days.map(d => {
            gained += d.gained;
            lost += d.lost;
            // The game's page host may draw no native tooltip: the same text in data-tip is drawn by the page on hover.
            const tip = tx('journal.chart.tip', { day: d.day, gained: format.points(d.gained), lost: format.points(d.lost) });
            return '<span class="jday" title="' + tip + '" data-tip="' + tip + '"><span class="jbar">'
                + '<span class="up">' + bar('gain', d.gained) + '</span><span class="down">' + bar('loss', d.lost) + '</span></span></span>';
        }).join('');
        const first = days[0].day, last = days[days.length - 1].day;
        return '<section class="jcard glass"><h3 class="eyebrow">' + tx('journal.chart') + '</h3><div class="jbars">' + bars + '</div>'
            + '<div class="jaxis"><span>' + tx('journal.day', { day: first }) + '</span>' + (last !== first ? '<span>' + tx('journal.day', { day: last }) + '</span>' : '') + '</div>'
            + '<p class="jtotals">' + tx('journal.chart.totals', { gained: format.points(gained), lost: format.points(lost) }) + '</p></section>';
    }

    function entriesHtml(list, none) {
        if (!list.length) return '<li class="none">' + none + '</li>';
        return list.map(c => '<li class="jentry"><b class="delta' + (c.points < 0 ? ' minus' : '') + '">' + format.signed(c.points) + '</b>'
            + '<span class="text"><span class="label">' + esc(c.label) + '</span><span class="when">' + tx('journal.day', { day: c.day }) + '</span></span></li>').join('');
    }

    function momentsHtml(d) {
        if (!d.gains.length && !d.losses.length) return '';
        return '<section class="jcard glass"><h3 class="eyebrow">' + tx('journal.moments') + '</h3><div class="jcols">'
            + '<div class="jcol"><h4 class="deepens">' + tx('journal.moments.gains') + '</h4><ul class="jlist">' + entriesHtml(d.gains, tx('journal.moments.none')) + '</ul></div>'
            + '<div class="jcol"><h4 class="wound">' + tx('journal.moments.losses') + '</h4><ul class="jlist">' + entriesHtml(d.losses, tx('journal.moments.none')) + '</ul></div>'
            + '</div></section>';
    }

    function historyHtml(list) {
        if (!list.length) return '';
        return '<section class="jcard glass"><h3 class="eyebrow">' + tx('journal.history') + '</h3><ul class="jlist">' + entriesHtml(list, '') + '</ul></section>';
    }

    // The whole pane for one bond. data: null while waiting for the answer, else the answer (normalized here, so
    // a raw one is safe too); found false (or anything that is not an answer) is the failure.
    function journalHtml(row, data) {
        const d = data === null || data === undefined ? null : normalize(data);
        const facts = d && d.found ? '<span class="since">' + esc(d.began ? S.t('journal.since', { day: d.began, together: together(d.daysTogether) }) : together(d.daysTogether)) + '</span>' : '';
        const head = '<header class="jhead glass"><div class="titles"><span class="eyebrow">' + tx('journal.title') + '</span>'
            + '<h2 class="jname">' + esc(row && row.name) + '</h2>'
            + '<p class="jfacts"><em class="jtier foil">' + esc(row && tiers.shown(row.levelName)) + '</em>' + facts + '</p></div>'
            + '<button class="btn" data-action="journal.back"><span class="glyph back"></span>' + tx('common.back') + '</button></header>';
        let body;
        if (data === null || data === undefined) body = '<p class="jnote">' + tx('journal.opening') + '</p>';
        else if (!d || !d.found) body = '<p class="jnote">' + tx('journal.failed') + '</p>';
        else if (isEmpty(d)) body = '<p class="jnote">' + tx('journal.empty') + '</p>';
        else body = milestonesHtml(d.milestones) + chartHtml(d.days) + momentsHtml(d) + historyHtml(d.history);
        return head + '<div class="jbody scroller" tabindex="0" data-ml-id="body-journal">' + body + '</div>';
    }

    const api = { normalize, journalHtml, LIMITS, WAIT_MS };
    if (node) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.journal = api;

    /* ---------- DOM ---------- */
    const RM = window.RM;
    const app = () => RM.app;
    // The open journal: { identity, data } (data null while waiting, { found: false } after the wait ran out).
    let open = null, timer = 0, painted = '';

    function stopWaiting() { clearTimeout(timer); timer = 0; }
    function close() { stopWaiting(); open = null; painted = ''; }

    function rowOf(state, identity) {
        return RM.state.visibleRows(state).find(r => RM.state.rowIdentity(r) === identity) || null;
    }

    function render(state, view) {
        const pane = document.getElementById('pane');
        if (!pane) return;
        if (view.pane !== 'journal' || !open) { if (open) close(); return; }
        const row = rowOf(state, open.identity);
        if (!row) return;
        const html = journalHtml(row, open.data);
        if (html === painted && pane.querySelector('.jbody')) return;
        const fresh = painted === '' || !pane.querySelector('.jbody');
        const body = pane.querySelector('.jbody'), top = !fresh && body ? body.scrollTop : 0;
        pane.className = fresh ? 'journal enter' : 'journal';
        pane.innerHTML = html;
        painted = html;
        const now = pane.querySelector('.jbody');
        if (now && top) now.scrollTop = top;
        if (fresh) setTimeout(() => { if (pane.className === 'journal enter') pane.className = 'journal'; }, 900);
    }

    function onAnswer(raw) {
        const data = normalize(raw);
        if (!data || !open || data.identity !== open.identity) return;
        stopWaiting();
        open.data = data;
        app().render();
    }

    const actions = {
        'journal.open': el => {
            const identity = el ? el.getAttribute('data-target') : null;
            if (!identity) return;
            stopWaiting();
            open = { identity, data: null };
            painted = '';
            app().view.pane = 'journal';
            app().render();
            // Before the request: an answer can arrive inside send().
            timer = setTimeout(() => {
                timer = 0;
                if (open && open.identity === identity && open.data === null) { open.data = { found: false }; app().render(); }
            }, WAIT_MS);
            RM.bridge.send('romantasyJournalOpen', { identity });
        },
        'journal.back': () => { app().view.pane = 'dossier'; app().render(); }
    };

    function init(appApi) {
        RM.bridge.on('romantasyJournal', onAnswer);
        // A state that moves the selection off this bond (it was removed, or hidden as away) ends the journal.
        appApi.on('state', () => {
            if (appApi.view.pane === 'journal' && (!open || appApi.view.selected !== open.identity)) {
                appApi.view.pane = 'dossier';
                appApi.render();
            }
        });
    }

    (RM.parts = RM.parts || []).push({ name: 'journal', render, actions, init });
})();
