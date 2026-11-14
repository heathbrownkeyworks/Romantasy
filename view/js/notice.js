// Notice toast: when a host message is shown and whether it reads as a warning, and
// render(), which shows the message as a pill rising from the bottom edge.
(function () {
    'use strict';
    const HOLD_MS = 3400;
    function isWarning(message) {
        const lower = String(message === null || message === undefined ? '' : message).toLowerCase();
        return lower.includes('could not') || lower.includes('cannot') || lower.includes('protected');
    }
    const shouldShow = (state, view) => state.revision !== view.lastRevision && !!state.message;

    const api = { HOLD_MS, isWarning, shouldShow };
    if (typeof module === 'object' && module.exports) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.notice = api;

    /* ---------- DOM ---------- */
    const RM = window.RM;
    let holdTimer = 0;

    function render(state, view) {
        const show = shouldShow(state, view);
        view.lastRevision = state.revision;
        if (!show) return;
        const el = document.getElementById('notice');
        if (!el) return;
        el.innerHTML = '<span class="pill"><i class="mark"></i><span class="text">' + RM.format.esc(state.message) + '</span></span>';
        el.className = isWarning(state.message) ? 'warning' : '';
        void el.offsetWidth;   // let the pill rise again when a notice replaces a notice
        el.classList.add('show');
        clearTimeout(holdTimer);
        holdTimer = setTimeout(() => { holdTimer = 0; el.classList.remove('show'); }, HOLD_MS);
    }

    api.render = render;
    (RM.parts = RM.parts || []).push({ name: 'notice', render });
})();
