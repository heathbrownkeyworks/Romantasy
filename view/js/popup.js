// The level-change popup: "Romance Deepened" or "Romance Wounded", over the ledger or alone
// over live play (popup mode). popupHtml and same are pure and run under Node for the tests;
// the DOM part below the export line paints #popup from view.popup.
//
// view.popup is null or the normalized change on screen:
//   { followerName, changeDirection: 'gain' | 'loss', previousLevelName, previousLevelIndex,
//     levelName, levelIndex, nextLevelName, points, pointsDelta, unlocks, seq }
// (strings are '' and numbers null when the payload lacked them; seq is 0 when unusable; unlocks is
// the lines of every tier gained, lowest first, [] on a loss or from an older plugin).
// Page -> plugin: romantasyPopupShown { seq } once it has been painted, romantasyPopupDismissed
// { seq } once it has gone. Action: popup.dismiss (the button and the scrim).
(function () {
    'use strict';
    const node = typeof module === 'object' && module.exports;
    const format = node ? require('./format.js') : window.RM.format;
    const tiers = node ? require('./tiers.js') : window.RM.tiers;
    const S = node ? require('./strings.js') : window.RM.strings;
    const esc = format.esc;
    const tx = (key, values) => esc(S.t(key, values));
    const AUTO_DISMISS_MS = 20000;
    const CLOSE_MS = 220;
    // Shown goes out on the second painted frame, or this long after the paint if frames do not come.
    const SHOWN_FALLBACK_MS = 250;
    const MAX_SEQ = 2147483647;

    const text = v => typeof v === 'string' ? v : '';
    const number = v => typeof v === 'number' && isFinite(v) ? v : null;
    const icon = key => '<img class="art" src="icons/' + key + '.png" alt="" draggable="false">';

    function normalize(raw) {
        const c = raw && typeof raw === 'object' && !Array.isArray(raw) ? raw : {};
        const seq = c.seq;
        return {
            followerName: text(c.followerName),
            changeDirection: c.changeDirection === 'loss' ? 'loss' : 'gain',   // as C++ chooses the music
            previousLevelName: text(c.previousLevelName),
            previousLevelIndex: number(c.previousLevelIndex),
            levelName: text(c.levelName),
            levelIndex: number(c.levelIndex),
            nextLevelName: text(c.nextLevelName),
            points: number(c.points),
            pointsDelta: number(c.pointsDelta),
            unlocks: Array.isArray(c.unlocks) ? c.unlocks.filter(u => typeof u === 'string') : [],
            seq: Number.isInteger(seq) && seq >= 1 && seq <= MAX_SEQ ? seq : 0
        };
    }

    // The tier's icon key. The name is exact (one the plugin sends); the level number (RomanceManager's, 1 = Stranger)
    // stands in when the name is missing or unknown.
    function tierKey(name, index) {
        const byName = tiers.TIERS[tiers.levelOf(name) - 1];
        if (byName) return byName.key;
        const byIndex = Number.isInteger(index) ? tiers.TIERS[index - 1] : null;
        return byIndex ? byIndex.key : null;
    }

    // 16 hearts thrown out from the trophy, at fixed angles so the markup stays pure.
    function burst() {
        let html = '';
        for (let i = 0; i < 16; i++) {
            const angle = i * Math.PI / 8 + (i % 2 ? .2 : 0), reach = i % 3 === 0 ? 250 : i % 3 === 1 ? 190 : 140;
            html += '<i class="bit" style="--tx:' + Math.round(Math.cos(angle) * reach) + 'px;--ty:' + Math.round(Math.sin(angle) * reach * .8)
                + 'px;--r:' + (i * 47 % 360) + 'deg;--s:' + (.6 + (i % 4) * .15).toFixed(2) + '">' + icon('likes') + '</i>';
        }
        return '<div class="burst">' + html + '</div>';
    }

    function tier(name, index) {
        const key = tierKey(name, index);
        return (key ? icon(key) : '') + '<span>' + esc(tiers.shown(name)) + '</span>';
    }

    function popupHtml(raw) {
        const c = normalize(raw), isLoss = c.changeDirection === 'loss';
        const shift = c.previousLevelName || c.levelName || c.previousLevelIndex !== null || c.levelIndex !== null
            ? '<p class="shift"><span class="from">' + tier(c.previousLevelName, c.previousLevelIndex) + '</span>'
                + '<i class="arrow">→</i>'
                + '<strong class="to' + (isLoss ? '' : ' foil') + '">' + tier(c.levelName, c.levelIndex) + '</strong></p>'
            : '';
        const figures = c.pointsDelta !== null || c.points !== null
            ? '<p class="figures">' + (c.pointsDelta !== null ? '<span class="delta">' + esc(format.signed(c.pointsDelta)) + '</span>' : '')
                + (c.points !== null ? '<span class="total">' + tx('popup.points', { points: format.points(c.points) }) + '</span>' : '') + '</p>'
            : '';
        return '<div class="scrim" data-action="popup.dismiss"></div>'
            + '<div class="sheet levelup ' + c.changeDirection + '" role="dialog" aria-modal="true">'
            + (isLoss ? '' : '<div class="rays"></div>')
            + '<div class="trophy">' + (isLoss ? icon('heartbroken') : burst() + icon('heart')) + '</div>'
            + '<p class="eyebrow">' + tx(isLoss ? 'popup.loss' : 'popup.gain') + '</p>'
            + (c.followerName ? '<h2>' + esc(c.followerName) + '</h2>' : '')
            + shift
            + (!isLoss && c.unlocks.length ? '<div class="unlocks"><p class="eyebrow">' + tx('popup.unlocked') + '</p><ul class="scroller" tabindex="0">'
                + c.unlocks.map(line => '<li>' + esc(line) + '</li>').join('') + '</ul></div>' : '')
            + figures
            + (c.nextLevelName ? '<p class="next">' + tx('popup.next', { tier: tiers.shown(c.nextLevelName) }) + '</p>' : '')
            + '<button class="btn' + (isLoss ? '' : ' primary') + '" data-action="popup.dismiss">' + tx('popup.continue') + '</button>'
            + '<div class="fuse"><i></i></div></div>';
    }

    // The same popup: C++ re-publishes the one on screen on a mode switch and on romantasyReady.
    function same(a, b) {
        if (!a || !b) return false;
        return a.followerName === b.followerName && a.levelName === b.levelName
            && a.changeDirection === b.changeDirection && a.points === b.points;
    }

    const api = { AUTO_DISMISS_MS, CLOSE_MS, SHOWN_FALLBACK_MS, normalize, popupHtml, same };
    if (node) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.popup = api;

    /* ---------- DOM ---------- */
    const RM = window.RM;
    // The popup this part last painted and its timers. view.popup is the truth; when it no
    // longer matches `painted` (cleared by app.js, replaced by show), the old one is let go silently.
    let painted = null, fuse = 0, closeTimer = 0, shownTimer = 0, closing = false, lastDismissedSeq = 0;

    const host = () => document.getElementById('popup');

    function show(raw) {
        if (!raw || typeof raw !== 'object' || Array.isArray(raw)) return;
        const change = normalize(raw);
        const view = RM.app.view;
        if (same(change, view.popup) && change.seq === view.popup.seq) return;
        if (change.seq > 0 && change.seq === lastDismissedSeq) return;   // a late re-publish of one already gone
        view.popup = change;
        RM.app.render();
    }

    function paint(state, view) {
        const el = host();
        if (!el) return;
        const popup = view.popup;
        if (popup === painted) return;
        const motionPlayed = closing;
        clearTimeout(fuse);
        fuse = 0;
        clearTimeout(shownTimer);
        shownTimer = 0;
        closing = false;
        if (!popup) {
            painted = null;
            clearTimeout(closeTimer);
            closeTimer = 0;
            if (motionPlayed) { el.className = ''; el.innerHTML = ''; return; }   // dismiss() already played it
            el.classList.add('closing');
            clearTimeout(closeTimer);
            // Cleared by timeout, not animationend: an event may never come while the view is hidden.
            closeTimer = setTimeout(() => { closeTimer = 0; el.className = ''; el.innerHTML = ''; }, CLOSE_MS);
            return;
        }
        clearTimeout(closeTimer);
        closeTimer = 0;
        painted = popup;
        el.innerHTML = popupHtml(popup);
        el.className = 'open ' + popup.changeDirection;
        const button = el.querySelector('.btn');
        if (button) button.focus();
        fuse = setTimeout(() => { fuse = 0; dismiss(); }, AUTO_DISMISS_MS);
        // Shown once the card has been painted: the frame that draws it, then the next. A page that
        // gets no frames (Ultralight may not run them for it) reports it by the timer instead. Whichever
        // comes first sends it, once, and only for this popup while it is up.
        let reported = false;
        const report = () => {
            if (reported || RM.app.view.popup !== popup || painted !== popup || closing) return;
            reported = true;
            clearTimeout(shownTimer);
            shownTimer = 0;
            RM.bridge.send('romantasyPopupShown', { seq: popup.seq });
        };
        shownTimer = setTimeout(() => { shownTimer = 0; report(); }, SHOWN_FALLBACK_MS);
        requestAnimationFrame(() => requestAnimationFrame(report));
    }

    // The button, the scrim, the fuse (and Task 11's keys). Idempotent: the closing motion plays,
    // then view.popup clears and Dismissed goes out, once. Sent also when Shown has not gone out
    // yet: C++ drops a Requested head on Dismissed instead of waiting out its 10 s timeout.
    function dismiss() {
        const popup = RM.app.view.popup, el = host();
        if (!popup || popup !== painted || closing) return;
        closing = true;
        clearTimeout(fuse);
        fuse = 0;
        if (el) el.classList.add('closing');
        clearTimeout(closeTimer);
        closeTimer = setTimeout(() => {
            closeTimer = 0;
            if (RM.app.view.popup !== popup) return;   // replaced or cleared meanwhile: C++ has moved on
            lastDismissedSeq = popup.seq;
            RM.app.view.popup = null;
            RM.app.render();
            RM.bridge.send('romantasyPopupDismissed', { seq: popup.seq });
        }, CLOSE_MS);
    }

    function init() {
        RM.bridge.on('romantasyLevelChange', show);
    }

    api.show = show;
    api.dismiss = dismiss;
    const actions = { 'popup.dismiss': () => dismiss() };
    (RM.parts = RM.parts || []).push({ name: 'popup', render: paint, actions, init });
})();
