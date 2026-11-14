// Enrollment: the full-stage takeover where the player creates a bond with a companion
// (create) or rewrites a player-created bond's personality (edit) by stamping deeds.
// The staging logic and the *Html functions are pure and run under Node for the tests;
// render() below the export line paints #takeover from view.takeover.
//
// view.takeover is null or { mode: 'create' | 'edit', identity, candidate, staged, query }:
//   identity   the bond being edited (edit), null (create)
//   candidate  the chosen candidate's identity (create), or null
//   staged     { editorID: 2 | 1 | -1 | -2 } for every stamped deed (favorite, like, dislike, pet peeve)
//   query      the deed search text
// Page events heard: open-enroll ({ mode: 'create' } | { mode: 'edit', identity }) from the ledger.
// Actions: enroll.pick (data-candidate), enroll.stamp (data-of, data-direction),
//          enroll.commit, enroll.cancel.
(function () {
    'use strict';
    const node = typeof module === 'object' && module.exports;
    const format = node ? require('./format.js') : window.RM.format;
    const model = node ? require('./state.js') : window.RM.state;
    const tiers = node ? require('./tiers.js') : window.RM.tiers;
    const S = node ? require('./strings.js') : window.RM.strings;
    const esc = format.esc;
    const tx = (key, values) => esc(S.t(key, values));

    const CATEGORY_ORDER = ['Battle', 'Blood & Beast', 'Crime & Transgression', 'Exploration & Growth', 'Influence', 'Magic & Craft', 'Quests & Allegiances'];
    // The jewel beside each category's header; a category not listed here gets none.
    const CATEGORY_ICON = {
        'Battle': 'cat-battle', 'Blood & Beast': 'cat-blood', 'Crime & Transgression': 'cat-crime', 'Exploration & Growth': 'cat-exploration',
        'Influence': 'cat-influence', 'Magic & Craft': 'cat-magic', 'Quests & Allegiances': 'cat-quests'
    };

    function groupByCategory(options) {
        const byCategory = new Map();
        for (const option of options) {
            if (!byCategory.has(option.category)) byCategory.set(option.category, []);
            byCategory.get(option.category).push(option);
        }
        const known = CATEGORY_ORDER.filter(c => byCategory.has(c));
        const unknown = [...byCategory.keys()].filter(c => !CATEGORY_ORDER.includes(c)).sort();
        return known.concat(unknown).map(category => ({ category, options: byCategory.get(category) }));
    }

    function matches(option, query) {
        const q = String(query === null || query === undefined ? '' : query).trim().toLowerCase();
        if (q === '') return true;
        return ((option.label || '') + ' ' + (option.category || '')).toLowerCase().includes(q);
    }

    // At most this many favorites, and as many pet peeves, per personality (the plugin refuses more).
    const CAP = 3;
    const CAP_LINE = { 2: S.t('enroll.cap.favorites'), '-2': S.t('enroll.cap.peeves') };

    // A favorite stages 2 and a pet peeve -2; past the cap, the rest stage as a like or dislike.
    function stagedFromRow(row, options) {
        const staged = {};
        const likes = row.likes || [], dislikes = row.dislikes || [], favorites = row.favorites || [], petPeeves = row.petPeeves || [];
        let fav = 0, peeve = 0;
        for (const option of options) {
            // The stamps can never stage a fourth favorite or pet peeve; this demotion only guards a row from elsewhere.
            if (favorites.includes(option.label) && fav < CAP) { staged[option.editorID] = 2; fav++; }
            else if (petPeeves.includes(option.label) && peeve < CAP) { staged[option.editorID] = -2; peeve++; }
            else if (likes.includes(option.label) || favorites.includes(option.label)) staged[option.editorID] = 1;
            else if (dislikes.includes(option.label) || petPeeves.includes(option.label)) staged[option.editorID] = -1;
        }
        return staged;
    }

    // -2 pet peeve, -1 dislike, 0 neutral, 1 like, 2 favorite.
    function directionOf(staged, id) {
        const v = staged[id];
        return v >= 2 ? 2 : v > 0 ? 1 : v <= -2 ? -2 : v < 0 ? -1 : 0;
    }
    // How many staged deeds carry this direction (2 or -2).
    function countOf(staged, direction) {
        let n = 0;
        for (const id of Object.keys(staged || {})) if (directionOf(staged, id) === direction) n++;
        return n;
    }
    // Whether the cap keeps this deed from taking a strong direction (2 or -2) it does not hold already.
    const capped = (staged, id, direction) => (direction === 2 || direction === -2) && directionOf(staged, id) !== direction && countOf(staged, direction) >= CAP;

    // Plain loop: options may come from another realm, and Array.map would inherit that realm's Array.
    // The page never sends more than three of either strong kind: any past the cap go as a like or dislike.
    function buildPage(options, staged) {
        const page = [];
        let fav = 0, peeve = 0;
        for (const o of options) {
            let d = directionOf(staged, o.editorID);
            // The stamps can never stage a fourth; this demotion only guards staged data from elsewhere.
            if (d === 2 && ++fav > CAP) d = 1;
            if (d === -2 && ++peeve > CAP) d = -1;
            page.push([o.editorID, d]);
        }
        return page;
    }

    function summary(options, staged) {
        const favorites = [], likes = [], dislikes = [], petPeeves = [];
        const into = { 2: favorites, 1: likes, '-1': dislikes, '-2': petPeeves };
        for (const option of options) {
            const d = directionOf(staged, option.editorID);
            if (d !== 0) into[d].push(deedLabel(option));
        }
        return { favorites, likes, dislikes, petPeeves };
    }

    // A deed row's key steps: neutral, like, favorite, dislike, pet peeve, neutral. full = { favorite, peeve }
    // says which strong kind is at its cap; its step is skipped.
    const ORDER = [0, 1, 2, -1, -2];
    function cycle(direction, full) {
        const f = full !== null && typeof full === 'object' ? full : {};
        let i = ORDER.indexOf(direction);
        for (let n = 0; n < ORDER.length; n++) {
            i = (i + 1) % ORDER.length;
            const d = ORDER[i];
            if (!(d === 2 && f.favorite) && !(d === -2 && f.peeve)) return d;
        }
        return 0;
    }

    function candidateMeta(candidate) {
        const parts = [candidate.role, candidate.sourcePlugin].filter(p => typeof p === 'string' && p !== '');
        return parts.length ? parts.join(' · ') : S.t('common.companion');
    }

    function canCommit(takeover) {
        if (takeover.mode === 'create' && !takeover.candidate) return false;
        return Object.keys(takeover.staged || {}).some(k => takeover.staged[k] !== 0);
    }

    /* ---------- the takeover's markup ---------- */

    const COPY = {
        create: { eyebrow: S.t('enroll.create.eyebrow'), title: S.t('enroll.create.title'), copy: S.t('enroll.create.copy'), commit: S.t('enroll.create.commit') },
        edit: { eyebrow: S.t('enroll.edit.eyebrow'), title: S.t('enroll.edit.title'), copy: S.t('enroll.edit.copy'), commit: S.t('enroll.edit.commit') }
    };
    const NOTE = S.t('enroll.note');
    const SUB = S.t('enroll.sub');
    const HINT = S.t('enroll.hint');
    const NO_CANDIDATES = S.t('enroll.nocandidates');
    const NO_DEEDS = S.t('enroll.nodeeds');
    // [direction, class, title, mark], in the order a deed row shows them
    const STAMPS = [
        [2, 'favorite', S.t('common.favorite'), '<img class="art" src="icons/star.png" alt="" draggable="false">'],
        [1, 'like', S.t('common.deepens'), '<img class="art" src="icons/likes.png" alt="" draggable="false">'],
        [0, 'neutral', S.t('enroll.neutral'), '<img class="art" src="icons/neutral.png" alt="" draggable="false">'],
        [-1, 'dislike', S.t('common.wounds'), '<img class="art" src="icons/dislikes.png" alt="" draggable="false">'],
        [-2, 'peeve', S.t('common.peeve'), '<img class="art" src="icons/heartbroken.png" alt="" draggable="false">']
    ];
    const DEED_CLASS = { 2: ' liked favored', 1: ' liked', '-1': ' disliked', '-2': ' disliked peeved' };
    // A deed's label as shown: strings.js deed.<editor ID> when it has that line, else the plugin's label (or the ID).
    function deedLabel(option) {
        const label = option.label || option.editorID;
        return typeof option.editorID === 'string' && option.editorID !== '' ? S.or('deed.' + option.editorID, label) : label;
    }

    const icon = key => '<img class="art" src="icons/' + key + '.png" alt="" draggable="false">';
    const isObject = v => v !== null && typeof v === 'object' && !Array.isArray(v);
    const optionsOf = state => (state.preferenceOptions || []).filter(o => isObject(o) && typeof o.editorID === 'string' && o.editorID !== '');
    const candidatesOf = state => (state.enrollmentCandidates || []).filter(isObject);
    const rowFor = (state, identity) => state.romances.find(row => model.rowIdentity(row) === identity) || null;

    function candidateHtml(candidate, i, chosen) {
        const identity = model.rowIdentity(candidate);
        return '<button class="row cand' + (identity === chosen ? ' selected' : '') + '" data-action="enroll.pick" data-candidate="' + esc(identity)
            + '" data-ml-id="cand-' + esc(identity) + '" aria-pressed="' + (identity === chosen ? 'true' : 'false') + '" style="--i:' + i + '">' + icon('stranger')
            + '<span class="who"><small class="role">' + esc(candidateMeta(candidate)) + '</small><b class="name">' + esc(candidate.name) + '</b></span></button>';
    }

    // The left column: the candidates (create) or the bond being edited (edit).
    function sideHtml(state, t) {
        if (t.mode === 'edit') {
            const row = rowFor(state, t.identity);
            if (!row) return '<h3 class="eyebrow">' + tx('enroll.companion') + '</h3><div class="bond"><b class="name">' + tx('enroll.notfound') + '</b><p class="copy">' + tx('enroll.notfound.copy') + '</p></div>';
            return '<h3 class="eyebrow">' + tx('enroll.companion') + '</h3><div class="bond">' + icon(tiers.tier(row.points).key)
                + '<small class="role">' + esc(row.role || S.t('common.companion')) + '</small><b class="name">' + esc(row.name) + '</b>'
                + '<em class="tiername">' + esc(tiers.shown(row.levelName)) + '</em><span class="pts">' + format.pointsWithThreshold(row.points) + '</span></div>';
        }
        const candidates = candidatesOf(state);
        return '<h3 class="eyebrow">' + tx('enroll.companion') + '<span class="count">' + candidates.length + '</span></h3><div class="cands scroll scroller">'
            + (candidates.length ? candidates.map((c, i) => candidateHtml(c, i, t.candidate)).join('') : '<p class="none">' + esc(NO_CANDIDATES) + '</p>')
            + '</div>';
    }

    // staged: every stamped deed, for the cap (a Favorite or Pet peeve stamp past it is disabled).
    function deedHtml(option, staged) {
        const id = esc(option.editorID), direction = directionOf(staged, option.editorID);
        return '<div class="deed' + (DEED_CLASS[direction] || '') + '" tabindex="0" data-deed="' + id + '" data-ml-id="deed-' + id + '" data-staged="' + direction + '">'
            + '<span class="label" title="' + esc(deedLabel(option)) + '">' + esc(deedLabel(option)) + '</span><span class="stamps">'
            + STAMPS.map(([d, cls, title, mark]) => {
                const off = capped(staged, option.editorID, d);
                return '<button class="stamp ' + cls + (d === direction ? ' on' : '') + '" data-action="enroll.stamp" data-of="' + id
                    + '" data-direction="' + d + '" data-ml-id="stamp-' + id + '-' + d + '" aria-pressed="' + (d === direction ? 'true' : 'false') + '"'
                    + (off ? ' disabled aria-disabled="true"' : '') + ' title="' + esc(off ? CAP_LINE[d] : title) + '">' + mark + '</button>';
            }).join('')
            + '</span></div>';
    }

    // The deed list, grouped by category and filtered by the search.
    function deedsHtml(state, t) {
        const options = optionsOf(state);
        if (!options.length) return '<p class="none">' + esc(NO_DEEDS) + '</p>';
        const query = String(t.query || '');
        const shown = options.filter(o => matches(o, query));
        if (!shown.length) return '<p class="none">' + tx('enroll.nomatch', { query: query.trim() }) + '</p>';
        const staged = t.staged || {};
        return groupByCategory(shown).map(group => '<section class="group"><h4 class="cat">'
            + (Object.prototype.hasOwnProperty.call(CATEGORY_ICON, group.category) ? icon(CATEGORY_ICON[group.category]) : '')
            + '<span class="catname">' + esc(group.category || S.t('enroll.category.other')) + '</span>'
            + '<span class="count">' + group.options.length + '</span></h4><div class="grid">'
            + group.options.map(o => deedHtml(o, staged)).join('') + '</div></section>').join('');
    }

    // strong: the favorites (or pet peeves), shown first with their mark and tag; then the rest.
    function chipsHtml(strong, labels, kind) {
        if (!strong.length && !labels.length) return '<li class="none">' + tx('enroll.chips.none') + '</li>';
        const [cls, key, word] = kind === 'favorite' ? ['fav', 'star', 'common.favorite'] : ['peeve', 'heartbroken', 'common.peeve'];
        return strong.map(label => '<li class="tag ' + cls + '">' + icon(key) + esc(label) + '<small class="kind">' + tx(word) + '</small></li>').join('')
            + labels.map(label => '<li class="tag">' + esc(label) + '</li>').join('');
    }
    // The cap line beside the list it limits, once that list is full.
    const capHtml = (count, direction) => count >= CAP ? '<span class="cap">' + esc(CAP_LINE[direction]) + '</span>' : '';

    // The bottom bar: the running summary and the commit and cancel buttons.
    function barHtml(state, t) {
        const sum = summary(optionsOf(state), t.staged || {});
        const copy = COPY[t.mode] || COPY.create;
        return '<div class="tally">'
            + '<div class="col like"><h4 class="eyebrow">' + tx('common.deepens') + '<b class="count">' + (sum.favorites.length + sum.likes.length) + '</b>'
            + capHtml(sum.favorites.length, 2) + '</h4><ul class="chips scroller" tabindex="0" data-ml-id="chips-like">' + chipsHtml(sum.favorites, sum.likes, 'favorite') + '</ul></div>'
            + '<div class="col dislike"><h4 class="eyebrow">' + tx('common.wounds') + '<b class="count">' + (sum.petPeeves.length + sum.dislikes.length) + '</b>'
            + capHtml(sum.petPeeves.length, -2) + '</h4><ul class="chips scroller" tabindex="0" data-ml-id="chips-dislike">' + chipsHtml(sum.petPeeves, sum.dislikes, 'peeve') + '</ul></div>'
            + '</div><div class="act"><p class="hint">' + esc(HINT) + '</p><div class="pair">'
            + '<button class="btn" data-action="enroll.cancel">' + tx('common.cancel') + '</button>'
            + '<button class="btn primary" data-action="enroll.commit"' + (canCommit(t) ? '' : ' disabled') + '>' + esc(copy.commit) + '</button></div></div>';
    }

    const pieces = (state, t) => ({ side: sideHtml(state, t), deeds: deedsHtml(state, t), bar: barHtml(state, t) });

    function shellHtml(t, p) {
        const copy = COPY[t.mode] || COPY.create;
        return '<div class="sheet enroll ' + (t.mode === 'edit' ? 'edit' : 'create') + '">'
            + '<header class="head"><div class="titles"><span class="eyebrow">' + esc(copy.eyebrow) + '</span><h2 class="foil">' + esc(copy.title) + '</h2>'
            + '<p class="copy">' + esc(copy.copy) + '</p></div><p class="note">' + esc(NOTE) + '</p></header>'
            + '<aside class="side">' + p.side + '</aside>'
            + '<section class="book"><div class="tools"><div class="titles"><h3 class="eyebrow">' + tx('enroll.deeds') + '</h3><p class="sub">' + esc(SUB) + '</p></div>'
            + '<label class="search"><img class="art seek" src="icons/search.png" alt="" draggable="false"><input class="query" type="text" autocomplete="off" spellcheck="false" placeholder="' + tx('enroll.search') + '" aria-label="' + tx('enroll.search') + '" value="'
            + esc(t.query || '') + '"></label></div>'
            + '<div class="deeds scroll scroller">' + p.deeds + '</div></section>'
            + '<footer class="ebar">' + p.bar + '</footer></div>';
    }

    const takeoverHtml = (state, t) => shellHtml(t, pieces(state, t));

    // What a new state leaves of an open takeover: null once the edited bond is gone;
    // the choice dropped once the chosen candidate is no longer offered.
    function reconcile(state, t) {
        if (!t) return null;
        if (t.mode === 'edit') return rowFor(state, t.identity) ? t : null;
        if (t.candidate !== null && t.candidate !== undefined && !candidatesOf(state).some(c => model.rowIdentity(c) === t.candidate)) {
            return Object.assign({}, t, { candidate: null });
        }
        return t;
    }

    const api = {
        CATEGORY_ORDER, CATEGORY_ICON, groupByCategory, matches, stagedFromRow, buildPage, summary, cycle, candidateMeta, canCommit,
        takeoverHtml, sideHtml, deedsHtml, barHtml, reconcile
    };
    if (node) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.enrollment = api;

    /* ---------- DOM ---------- */
    const RM = window.RM;
    const SLOTS = { side: '.side', deeds: '.deeds', bar: '.ebar' };
    let painted = null;   // { key, side, deeds, bar }: what #takeover shows now
    let enterTimer = 0;
    // How long the sheet keeps .enter: the candidate rows' staggered entrance plays only then,
    // so later repaints of the column (a selection, a new candidate) do not replay it.
    const ENTER_MS = 900;

    const host = () => document.getElementById('takeover');

    // How to find a control again after its list is repainted.
    function focusKey(el) {
        for (const attr of ['data-of', 'data-deed', 'data-candidate', 'data-action']) {
            const value = el.getAttribute && el.getAttribute(attr);
            if (value !== null && value !== undefined) return { attr, value, direction: el.getAttribute('data-direction') };
        }
        return null;
    }
    function findKey(root, key) {
        for (const el of root.querySelectorAll('[' + key.attr + ']')) {
            if (el.getAttribute(key.attr) === key.value && el.getAttribute('data-direction') === key.direction) return el;
        }
        return null;
    }

    // Replace a slot's markup, keeping its scroll position and the focused control.
    function swap(el, html) {
        if (!el) return;
        const scroller = el.querySelector('.scroll') || el;
        const top = scroller.scrollTop;
        const active = document.activeElement;
        const key = active && active !== el && el.contains(active) ? focusKey(active) : null;
        el.innerHTML = html;
        (el.querySelector('.scroll') || el).scrollTop = top;
        const again = key && findKey(el, key);
        if (again) again.focus({ preventScroll: true });
    }

    function paint(state, view) {
        const el = host();
        if (!el) return;
        if (view.takeover) view.takeover = reconcile(state, view.takeover);
        const t = view.takeover;
        // While a takeover is open the notice rides above its bottom bar (base.css).
        document.documentElement.classList.toggle('enrolling', !!t);
        if (!t) {
            if (!painted) return;
            painted = null;
            clearTimeout(enterTimer);
            enterTimer = 0;
            el.className = '';
            el.innerHTML = '';
            return;
        }
        const next = pieces(state, t), key = t.mode + '|' + (t.identity || '');
        if (!painted || painted.key !== key) {
            el.innerHTML = shellHtml(t, next);
            el.className = 'open';
            painted = Object.assign({ key }, next);
            const sheet = el.querySelector('.sheet');
            if (sheet) sheet.classList.add('enter');
            clearTimeout(enterTimer);
            enterTimer = setTimeout(() => { enterTimer = 0; if (sheet) sheet.classList.remove('enter'); }, ENTER_MS);
            return;
        }
        for (const slot of Object.keys(SLOTS)) {
            if (next[slot] === painted[slot]) continue;
            swap(el.querySelector(SLOTS[slot]), next[slot]);
            painted[slot] = next[slot];
        }
    }

    function open(mode, identity) {
        const app = RM.app;
        let staged = {};
        if (mode === 'edit') {
            const row = rowFor(app.state, identity);
            if (!row) return;
            staged = stagedFromRow(row, optionsOf(app.state));
        } else if (mode !== 'create') {
            return;
        }
        app.view.takeover = { mode, identity: mode === 'edit' ? identity : null, candidate: null, staged, query: '' };
        app.render();
    }

    function close() {
        RM.app.view.takeover = null;
        RM.app.render();
    }

    function stage(id, direction) {
        const t = RM.app.view.takeover;
        if (!t || typeof id !== 'string') return;
        const staged = Object.assign({}, t.staged);
        if (direction === 0) delete staged[id];
        else staged[id] = direction;
        t.staged = staged;
        RM.app.render();
        // Only the stamp just pressed pops; the repaint redraws every other stamp still.
        const el = host(), row = el && [...el.querySelectorAll('[data-deed]')].find(n => n.getAttribute('data-deed') === id);
        if (row) row.classList.add('stamped');
    }

    // Sends the page; closes only once the host has it.
    function commit() {
        const app = RM.app, t = app.view.takeover;
        if (!t || !canCommit(t)) return;
        const payload = {
            kind: t.mode === 'create' ? 'enroll' : 'replace',
            identity: t.mode === 'create' ? t.candidate : t.identity,
            preferences: buildPage(optionsOf(app.state), t.staged)
        };
        if (RM.bridge.send('romantasyBond', payload) !== true) return;
        close();
    }

    // Typing filters the deed list only: the search box is never repainted.
    function onInput(event) {
        const t = RM.app.view.takeover, target = event.target;
        if (!t || !painted || !target || !target.classList || !target.classList.contains('query')) return;
        t.query = String(target.value || '');
        const list = host().querySelector('.deeds'), html = deedsHtml(RM.app.state, t);
        if (!list || html === painted.deeds) return;
        list.innerHTML = html;
        list.scrollTop = 0;
        painted.deeds = html;
    }

    // A deed row's own key (Enter, or gamepad A, through pad.js) moves its stamp round:
    // neutral, like, favorite, dislike, pet peeve, skipping a kind at its cap.
    function stampNext(id) {
        const t = RM.app.view.takeover;
        if (!t || typeof id !== 'string') return;
        const staged = t.staged || {};
        stage(id, cycle(directionOf(staged, id), { favorite: countOf(staged, 2) >= CAP, peeve: countOf(staged, -2) >= CAP }));
    }

    const actions = {
        'enroll.pick': el => {
            const t = RM.app.view.takeover;
            if (!t || t.mode !== 'create') return;
            t.candidate = el.getAttribute('data-candidate');
            RM.app.render();
        },
        'enroll.stamp': el => {
            const t = RM.app.view.takeover;
            if (!t) return;
            const id = el.getAttribute('data-of'), d = Number(el.getAttribute('data-direction'));
            if (!ORDER.includes(d) || capped(t.staged || {}, id, d)) return;
            stage(id, d !== 0 && directionOf(t.staged || {}, id) === d ? 0 : d);
        },
        'enroll.commit': () => commit(),
        'enroll.cancel': () => close()
    };

    function init(app) {
        app.on('open-enroll', payload => { if (payload) open(payload.mode, payload.identity); });
        const el = host();
        if (el) {
            el.addEventListener('input', onInput);
        }
    }

    Object.assign(api, { open, commit, cancel: close, stampNext, render: () => paint(RM.app.state, RM.app.view) });
    (RM.parts = RM.parts || []).push({ name: 'enrollment', render: paint, actions, init });
})();
