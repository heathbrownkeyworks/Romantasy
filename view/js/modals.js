// The three modal sheets: confirm reset, confirm remove, and the developer password.
// modalHtml is pure and runs under Node for the tests; render() below the export line
// paints #modal from view.modal.
//
// view.modal is null or one of:
//   { kind: 'reset' | 'remove', identity }
//   { kind: 'password', pending?: true, error?: true }
// Page events heard: confirm ({ kind, identity }) from the ledger, state (for the password reply).
// Actions: modal.confirm, modal.cancel, modal.unlock.
(function () {
    'use strict';
    const node = typeof module === 'object' && module.exports;
    const format = node ? require('./format.js') : window.RM.format;
    const model = node ? require('./state.js') : window.RM.state;
    const S = node ? require('./strings.js') : window.RM.strings;
    const esc = format.esc;
    const tx = key => esc(S.t(key));
    const CLOSE_MS = 220;

    const icon = key => '<img class="art" src="icons/' + key + '.png" alt="" draggable="false">';

    const CONFIRM = {
        reset: { title: S.t('modal.reset.title'), button: S.t('modal.reset.button'), jewel: 'stranger', body: S.t('modal.reset.body') },
        remove: { title: S.t('modal.remove.title'), button: S.t('modal.remove.button'), jewel: 'heartbroken', body: S.t('modal.remove.body') }
    };

    function confirmHtml(modal, row) {
        const copy = CONFIRM[modal.kind];
        return '<div class="sheet confirm" data-kind="' + modal.kind + '">'
            + '<div class="jewel">' + icon(copy.jewel) + '</div>'
            + '<p class="eyebrow">' + tx('modal.eyebrow') + '</p>'
            + '<h2>' + esc(copy.title) + '</h2>'
            + (row ? '<p class="who">' + esc(row.name) + '</p>' : '')
            + '<p class="note">' + esc(copy.body) + '</p>'
            + '<div class="pair"><button class="btn" data-action="modal.cancel">' + tx('common.cancel') + '</button>'
            + '<button class="btn ' + (modal.kind === 'remove' ? 'danger' : 'primary') + '" data-action="modal.confirm">' + esc(copy.button) + '</button></div></div>';
    }

    function passwordHtml(modal) {
        return '<div class="sheet password">'
            + '<div class="jewel">' + icon('settings') + '</div>'
            + '<p class="eyebrow">' + tx('modal.password.eyebrow') + '</p>'
            + '<h2>' + tx('modal.password.title') + '</h2>'
            + '<p class="note">' + tx('modal.password.note') + '</p>'
            + '<input type="password" class="field" autocomplete="off" spellcheck="false" aria-label="' + tx('modal.password.field') + '">'
            + (modal.error ? '<p class="error">' + tx('modal.password.error') + '</p>' : '')
            + '<div class="pair"><button class="btn" data-action="modal.cancel">' + tx('common.cancel') + '</button>'
            + '<button class="btn primary" data-action="modal.unlock">' + tx('modal.password.unlock') + '</button></div></div>';
    }

    function modalHtml(modal, row) {
        if (!modal) return '';
        if (modal.kind === 'password') return passwordHtml(modal);
        if (CONFIRM[modal.kind]) return confirmHtml(modal, row);
        return '';
    }

    const api = { modalHtml, CLOSE_MS };
    if (node) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.modals = api;

    /* ---------- DOM ---------- */
    const RM = window.RM;
    let paintedKey = null, closeTimer = 0;

    const host = () => document.getElementById('modal');
    const keyOf = modal => modal ? modal.kind + '|' + (modal.identity || '') : null;
    const rowFor = (state, identity) => state.romances.find(row => model.rowIdentity(row) === identity) || null;

    function paint(state, view) {
        const el = host();
        if (!el) return;
        const modal = view.modal, key = keyOf(modal);
        if (!modal) {
            if (paintedKey === null) return;
            paintedKey = null;
            el.classList.add('closing');
            clearTimeout(closeTimer);
            // Cleared by timeout, not animationend: an event may never come while the view is hidden.
            closeTimer = setTimeout(() => {
                closeTimer = 0;
                el.classList.remove('open');
                el.classList.remove('closing');
                el.innerHTML = '';
            }, CLOSE_MS);
            return;
        }
        if (key !== paintedKey) {
            clearTimeout(closeTimer);
            closeTimer = 0;
            el.innerHTML = modalHtml(modal, rowFor(state, modal.identity));
            el.className = 'open';
            paintedKey = key;
            // The field for the password; Cancel for Reset and Remove, so no key press confirms them by default.
            const first = el.querySelector(modal.kind === 'password' ? '.field' : '[data-action="modal.cancel"]');
            if (first) first.focus();
            return;
        }
        // Same sheet: the password sheet only shows or hides the error line and frees the button.
        if (modal.kind !== 'password') return;
        const field = el.querySelector('.field'), unlock = el.querySelector('[data-action="modal.unlock"]');
        if (unlock) unlock.disabled = modal.pending === true;
        const error = el.querySelector('.error');
        if (modal.error && !error && field) {
            field.insertAdjacentHTML('afterend', '<p class="error">' + tx('modal.password.error') + '</p>');
        } else if (!modal.error && error) {
            error.remove();
        }
    }

    // The sheet shakes once per refused password: called on the pending -> error step only.
    function shake() {
        const sheet = host() && host().querySelector('.sheet');
        if (!sheet) return;
        sheet.classList.remove('shake');
        void sheet.offsetWidth;   // restart the shake
        sheet.classList.add('shake');
    }

    function close() {
        RM.app.view.modal = null;
        RM.app.render();
    }

    // Sends the password and waits for the next state to answer.
    function submit(password) {
        const modal = RM.app.view.modal;
        if (!modal || modal.kind !== 'password' || modal.pending) return;
        modal.error = false;
        // Pending before the send: a host may answer inside send() (the browser harness does).
        modal.pending = true;
        const sent = RM.bridge.send('romantasyDevUnlock', { password: String(password === undefined || password === null ? '' : password) }) === true;
        if (!sent && RM.app.view.modal === modal) modal.pending = false;
        RM.app.render();
    }

    function onState(state) {
        const modal = RM.app.view.modal;
        // A Reset or Remove sheet goes with its bond: checked against every bond, not the visible roster.
        if (modal && CONFIRM[modal.kind] && !rowFor(state, modal.identity)) { close(); return; }
        if (!modal || modal.kind !== 'password') return;
        // Unlocked is the answer whenever it comes: also after an earlier state already read as a refusal.
        if (state.developerToolsUnlocked) { close(); return; }
        if (!modal.pending) return;
        modal.pending = false;
        modal.error = true;
        const field = host() && host().querySelector('.field');
        if (field) { field.value = ''; field.focus(); }
        RM.app.render();
        shake();
    }

    function init(app) {
        app.on('confirm', payload => {
            if (!payload || !CONFIRM[payload.kind]) return;
            app.view.modal = { kind: payload.kind, identity: payload.identity };
            app.render();
        });
        app.on('state', onState);
    }

    const actions = {
        'modal.cancel': () => close(),
        'modal.confirm': () => {
            const modal = RM.app.view.modal;
            if (!modal || !CONFIRM[modal.kind]) return;
            RM.bridge.send('romantasyBond', { kind: modal.kind, identity: modal.identity });
            close();
        },
        'modal.unlock': () => {
            const field = host() && host().querySelector('.field');
            submit(field ? field.value : '');
        }
    };

    RM.modals.submit = submit;
    (RM.parts = RM.parts || []).push({ name: 'modals', render: paint, actions, init });
})();
