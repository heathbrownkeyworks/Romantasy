// The page's one owner of keys and the gamepad, and the key legend.
// Pure rules (run under Node for the tests): layer(view) names the top of the back chain,
// back(view) the action Escape or B takes there, isCloseChord / isTyping, legend(view, device), and the
// scroll maths: nearest (bring a control into view) and nudge (one press on a scrolling body).
// Below the export line: one keydown listener on document; Magelight's gamepad helper (through
// RM.bridge.gamepad(), host 1.6.0) with one navigation scope per open layer; the legend in #legend.
//
// Back chain: popup -> modal -> takeover -> settings or journal -> ledger (close). Each step runs the action
// the matching button runs (popup.dismiss, modal.cancel, enroll.cancel, settings.back, journal.back, close), so a
// key and a click do the same thing. Keys and gamepad focus never reach a layer under the top one.
(function () {
    'use strict';
    const node = typeof module === 'object' && module.exports;
    const S = node ? require('./strings.js') : window.RM.strings;

    function layer(view) {
        if (view.popup) return 'popup';
        if (view.modal) return 'modal';
        if (view.takeover) return 'takeover';
        if (view.pane === 'settings') return 'settings';
        if (view.pane === 'journal') return 'journal';
        return 'ledger';
    }
    const BACK = { popup: 'dismiss-popup', modal: 'close-modal', takeover: 'close-takeover', settings: 'close-settings', journal: 'close-journal', ledger: 'close' };
    const back = view => BACK[layer(view)];

    // Left Ctrl + R as the game's hotkey reads it (InputMode.cpp): only while that hotkey is live
    // (not in Favorites mode) and never while a text field has the keys.
    const isCloseChord = (event, state, typing) => !!event && event.code === 'KeyR' && event.ctrlKey === true
        && !event.altKey && !event.shiftKey && !event.metaKey && !(state && state.openWithFavorites) && !typing;

    const isTyping = el => !!el && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') && !el.disabled;

    // [keyboard prompt, gamepad actions, label] per layer.
    const ENTER = S.t('legend.key.enter'), ESC = S.t('legend.key.esc');
    const LEGEND = {
        ledger: [['↑↓', ['up', 'down'], S.t('legend.select')], [ENTER, ['accept'], S.t('legend.open')], [ESC, ['cancel'], S.t('legend.close')]],
        settings: [[ENTER, ['accept'], S.t('legend.select')], [ESC, ['cancel'], S.t('legend.back')]],
        journal: [['↑↓', ['up', 'down'], S.t('legend.scroll')], [ESC, ['cancel'], S.t('legend.back')]],
        takeover: [[ENTER, ['accept'], S.t('legend.stamp')], [ESC, ['cancel'], S.t('legend.cancel')]],
        modal: [[ENTER, ['accept'], S.t('legend.confirm')], [ESC, ['cancel'], S.t('legend.cancel')]],
        popup: [[ENTER, ['accept'], S.t('legend.continue')]]
    };
    // Magelight's default bindings in Xbox labels, for when the helper has none to give.
    const DEFAULT_PROMPTS = { up: S.t('legend.pad.up'), down: S.t('legend.pad.down'), accept: 'A', cancel: 'B' };
    let promptOf = action => DEFAULT_PROMPTS[action] || '';

    function gamepadPrompt(actions) {
        const names = actions.map(a => promptOf(a) || DEFAULT_PROMPTS[a] || '');
        // Up and down on the d-pad read as one prompt: the helper's names start "D-pad ", the defaults are ours.
        const dpad = n => n.indexOf('D-pad ') === 0 || n === DEFAULT_PROMPTS.up || n === DEFAULT_PROMPTS.down;
        if (names.length === 2 && names.every(dpad)) return S.t('legend.pad.updown');
        return names.join(' ');
    }

    // Reset and Remove open on Cancel, so Enter there acts on the focused button: "Select".
    const DESTRUCTIVE = [[ENTER, ['accept'], S.t('legend.select')], [ESC, ['cancel'], S.t('legend.cancel')]];
    const isDestructive = view => !!view.modal && (view.modal.kind === 'reset' || view.modal.kind === 'remove');
    const legend = (view, device) => (layer(view) === 'modal' && isDestructive(view) ? DESTRUCTIVE : LEGEND[layer(view)]).map(([keys, actions, label]) =>
        ({ prompt: device === 'gamepad' ? gamepadPrompt(actions) : keys, label }));

    // Scrolling never reaches the page (it is a game screen): a control is brought into view by its nearest scrolling
    // ancestor only, by the least that shows it. top and bottom are the control's edges in that body's content.
    function nearest(scrollTop, clientHeight, top, bottom) {
        if (top < scrollTop) return top;
        if (bottom > scrollTop + clientHeight) return Math.min(top, bottom - clientHeight);
        return scrollTop;
    }
    // A body with no focus stop of its own (a values list, Recent changes) scrolls this many stage pixels a press.
    const STEP = 60;
    // Where one press moves a body: null when it is already at that end.
    function nudge(scrollTop, clientHeight, scrollHeight, delta) {
        const next = Math.max(0, Math.min(Math.max(0, scrollHeight - clientHeight), scrollTop + delta));
        return Math.abs(next - scrollTop) < 1 ? null : next;
    }

    const api = { layer, back, isCloseChord, isTyping, legend, nearest, nudge, STEP };
    if (node) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.pad = api;

    /* ---------- DOM ---------- */
    const RM = window.RM, root = document.documentElement;
    const FOCUSABLE = 'button,input,textarea,[tabindex]';
    // Escape's button for each step of the back chain: [host id, action].
    const BACK_ACTION = {
        'dismiss-popup': ['popup', 'popup.dismiss'], 'close-modal': ['modal', 'modal.cancel'],
        'close-takeover': ['takeover', 'enroll.cancel'], 'close-settings': ['pane', 'settings.back'],
        'close-journal': ['pane', 'journal.back'], 'close': ['roster', 'close']
    };
    const ROOTS = { ledger: ['roster', 'pane'], settings: ['roster', 'pane'], journal: ['roster', 'pane'], takeover: ['takeover'], modal: ['modal'], popup: ['popup'] };
    const ARROWS = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right', Up: 'up', Down: 'down', Left: 'left', Right: 'right' };
    let listening = false, helper = null, shownLayer = null, legendHtml = null;
    const swallowed = {};    // 'Enter' / ' ': a keydown this listener handled; its keypress and keyup go nowhere
    const scopes = [];       // the open navigation scopes, bottom first: { name, dispose }
    const remembered = {};   // the last control focused in each layer while it was on top

    const app = () => RM.app;
    const byId = id => document.getElementById(id);
    const rootsOf = name => (ROOTS[name] || []).map(byId).filter(Boolean);
    const within = (roots, el) => !!el && roots.some(r => r === el || r.contains(el));
    const live = el => !el.disabled && el.getAttribute('tabindex') !== '-1' && (!el.getClientRects || el.getClientRects().length > 0);
    // A scrolling body that is a focus stop itself (it holds none): tabindex 0 and the class scroller.
    const isBody = el => !!el && !!el.classList && el.classList.contains('scroller') && el.getAttribute('tabindex') === '0';
    // The choice row (role radiogroup) an option belongs to, or null for any other element.
    const rowOf = el => !!el && !!el.getAttribute && el.getAttribute('role') === 'radio' && el.closest ? el.closest('[role="radiogroup"]') : null;
    // A choice row is one stop: the option with the focus while the focus is in the row, else the checked option.
    function standsForRow(el) {
        const row = rowOf(el), active = document.activeElement;
        if (!row) return true;
        return rowOf(active) === row ? active === el : el.getAttribute('aria-checked') === 'true';
    }
    // A focus stop: a live control, or a scrolling body with something to scroll (one that fits, to a pixel, is passed over).
    const stop = el => live(el) && (!isBody(el) || el.scrollHeight > el.clientHeight + 1) && standsForRow(el);
    const liveIn = roots => roots.reduce((list, r) => list.concat([...r.querySelectorAll(FOCUSABLE)].filter(live)), []);
    // A layer's focus stops in page order, its scrolling bodies after its controls, so they never come between
    // controls the keys already walked in order.
    const order = all => all.filter(el => !isBody(el)).concat(all.filter(isBody));
    const controls = roots => order(liveIn(roots).filter(stop));
    // Where active sits in list: its index; for an element of the roots that is no stop (a body with nothing to scroll,
    // focused by a click) the half step between the stops either side of it, so a step lands on its neighbour; -1 outside.
    function place(list, active, roots) {
        const i = list.indexOf(active);
        if (i >= 0 || !active || !roots || !within(roots, active)) return i;
        const all = order(liveIn(roots)), j = all.indexOf(active);
        return j < 0 ? -1 : all.slice(0, j).filter(el => list.indexOf(el) >= 0).length - 0.5;
    }
    // The index one step from place p (an index or a half step).
    const beside = (p, delta) => delta > 0 ? Math.floor(p) + 1 : Math.ceil(p) - 1;
    const focus = el => {
        if (!el) return;
        if (el.focus) el.focus({ preventScroll: true });
        reveal(el);
        if (topLayer() === 'settings') reachEnds(el);
    };
    // The nearest ancestor that scrolls (overflow-y auto or scroll), or null: the only box a focus move may scroll.
    function scrollerOf(el) {
        for (let n = el.parentElement; n; n = n.parentElement) {
            const y = window.getComputedStyle(n).overflowY;
            if (y === 'auto' || y === 'scroll') return n;
        }
        return null;
    }
    // Brings el into view inside its nearest scrolling ancestor, by the least amount, and touches nothing above it.
    // Both rects are in the same units, scaled to the body's pixels, whichever way the engine reports a CSS zoom.
    function reveal(el) {
        const body = scrollerOf(el);
        if (!body) return;
        const r = el.getBoundingClientRect(), b = body.getBoundingClientRect();
        const k = b.height && body.offsetHeight ? body.offsetHeight / b.height : 1;
        const top = body.scrollTop + (r.top - b.top) * k - (body.clientTop || 0);
        const next = nearest(body.scrollTop, body.clientHeight, top, top + r.height * k);
        if (next !== body.scrollTop) body.scrollTop = next;
    }
    // One press of Up or Down on a scrolling body: true when it moved (at its end the press moves the focus on instead).
    function nudgeBody(el, dir) {
        const next = nudge(el.scrollTop, el.clientHeight, el.scrollHeight, dir === 'down' ? STEP : -STEP);
        if (next === null) return false;
        el.scrollTop = next;
        return true;
    }
    const actionButton = (hostId, action) => { const h = byId(hostId); return h ? h.querySelector('[data-action="' + action + '"]') : null; };
    const selectedRow = () => { const r = byId('roster'), row = r && r.querySelector('.row.selected'); return row && live(row) ? row : null; };

    // The top layer, or null while the page is hidden or, in popup mode, between two popups.
    function topLayer() {
        const view = app().view;
        if (view.mode === 'hidden') return null;
        const top = layer(view);
        return view.mode === 'popup' && top !== 'popup' ? null : top;
    }

    // The control a layer starts on: what the gamepad's initialFocus and a keyboard return land on.
    function primary(name) {
        const first = (hostId, selectors) => {
            const h = byId(hostId);
            for (const sel of selectors) { const hit = h && [...h.querySelectorAll(sel)].find(live); if (hit) return hit; }
            return null;
        };
        if (name === 'ledger') return selectedRow() || first('roster', ['.row']) || first('pane', ['button']);
        if (name === 'settings') return first('pane', ['[role="switch"]', 'button']);
        if (name === 'journal') return first('pane', ['[data-action="journal.back"]', 'button']);
        if (name === 'takeover') return first('takeover', ['.cand.selected', '.cand', '.deed', 'button']);
        // Reset and Remove start on Cancel (a destructive sheet never defaults to its action); the password on Unlock.
        if (name === 'modal') return first('modal', ['[data-action="modal.unlock"]', '[data-action="modal.cancel"]', 'button']);
        if (name === 'popup') return first('popup', ['.btn']);
        return null;
    }

    // Runs a control's action as its click would, sound and all.
    function press(hostId, action) {
        const el = actionButton(hostId, action);
        if (el) { app().activate(el); return; }
        RM.bridge.sound('click');
        app().dispatch(action, null);
    }

    function runBack() {
        const top = topLayer();
        if (!top) return;
        const [hostId, action] = BACK_ACTION[back(app().view)];
        press(hostId, action);
    }

    // Up and Down in the ledger: the selection moves through the visible rows, as a click on the
    // row would move it, and stops at either end. The repainted row takes the focus.
    function moveSelection(delta) {
        const a = app(), rows = RM.roster.shownRows(a.state, a.view);
        if (!rows.length) return;
        let index = rows.findIndex(row => RM.state.rowIdentity(row) === a.view.selected);
        if (index < 0) index = Math.min(Math.max(0, a.view.selectedIndex || 0), rows.length - 1);
        const next = Math.max(0, Math.min(rows.length - 1, index + delta));
        const identity = RM.state.rowIdentity(rows[next]);
        if (next !== index || identity !== a.view.selected || a.view.pane !== 'dossier') a.dispatch('select', rowButton(identity));
        focus(selectedRow());
    }
    function rowButton(identity) {
        const r = byId('roster');
        return (r && [...r.querySelectorAll('[data-identity]')].find(el => el.getAttribute('data-identity') === identity)) || null;
    }

    // One step through a list of controls, no wrap, from where the focus is in the roots; from outside them, the
    // list's first (or last) one.
    function step(list, active, delta, roots) {
        if (!list.length) return;
        const p = place(list, active, roots);
        focus(p === -1 ? list[delta > 0 ? 0 : list.length - 1] : list[Math.max(0, Math.min(list.length - 1, beside(p, delta)))]);
    }

    // Left or Right on an option: the focus moves to the option beside it and never leaves the row (choosing is
    // Enter, Space, A or a click). True when el is an option, so the press is spent even at the row's end.
    function optionStep(el, dir) {
        const row = rowOf(el);
        if (!row) return false;
        const options = [...row.querySelectorAll('[role="radio"]')].filter(live);
        const next = options[options.indexOf(el) + (dir === 'right' ? 1 : -1)];
        if (next) focus(next);
        return true;
    }

    function arrow(top, dir, active) {
        const delta = dir === 'down' || dir === 'right' ? 1 : -1;
        if ((dir === 'up' || dir === 'down') && isBody(active) && within(rootsOf(top), active) && nudgeBody(active, dir)) return;
        if ((dir === 'left' || dir === 'right') && within(rootsOf(top), active) && optionStep(active, dir)) return;
        if (top === 'ledger') {
            const pane = byId('pane'), paneList = pane ? controls([pane]) : [];
            const inPane = within(pane ? [pane] : [], active);
            // The roster head (the order choice, the clear button) sits above the rows: Down goes to the selected row.
            const inHead = !inPane && !!active && !!active.closest && !!active.closest('.rhead');
            if (inHead && (dir === 'up' || dir === 'down')) { if (dir === 'down') focus(primary('ledger')); return; }
            if (!inPane && (dir === 'up' || dir === 'down')) { moveSelection(delta); return; }
            if (!inPane) { if (dir === 'right') step(paneList, null, 1); return; }
            if (dir === 'left' && paneList.indexOf(active) === 0) { focus(selectedRow()); return; }
            step(paneList, active, delta, [pane]);
            return;
        }
        if (top === 'settings' || top === 'journal') { const pane = byId('pane'); step(pane ? controls([pane]) : [], active, delta, pane ? [pane] : []); return; }
        // Takeover and modal: through the sheet's controls, leaving out its text field; Up and Down
        // pass over the stamps, so they walk the candidates and the deed rows.
        let list = controls(rootsOf(top)).filter(el => !isTyping(el));
        if (top === 'takeover' && (dir === 'up' || dir === 'down')) list = list.filter(el => !el.classList.contains('stamp'));
        step(list, active, delta, rootsOf(top));
    }

    // Enter or Space on the focused control of the top layer; Enter elsewhere on a modal confirms it.
    function activate(top, key, active) {
        const roots = rootsOf(top);
        const inside = within(roots, active) ? active : null;
        const deed = inside ? inside.getAttribute('data-deed') : null;
        if (deed !== null) { RM.enrollment.stampNext(deed); return; }
        const hit = inside && inside.closest('[data-action],[data-identity]');
        if (hit && within(roots, hit)) {
            const row = hit.getAttribute('data-identity') !== null && (top === 'ledger' || top === 'journal');
            app().activate(hit);
            if (row) focus(selectedRow());
            return;
        }
        if (key === 'Enter' && top === 'modal') {
            // The password sheet's Unlock only (disabled while the answer is pending); Reset and Remove do nothing
            // without a focused button.
            app().activate(actionButton('modal', 'modal.unlock'));
        }
    }

    function onKeyDown(event) {
        if (event.isComposing || event.keyCode === 229) return;
        useKeyboard();
        const top = topLayer();
        if (!top) {
            // Keys reach a page that believes it is hidden only when it is on screen after all (it missed a
            // mode): Escape asks the plugin to close, which it does only if something is open.
            const isEscape = event.key === 'Escape' || event.key === 'Esc';
            if (isEscape && !event.repeat && app().view.mode === 'hidden' && !app().releasePending()) {
                event.preventDefault();
                RM.bridge.send('romantasyClose');
            }
            return;
        }
        const key = event.key, active = document.activeElement, typing = isTyping(active);
        const pressKey = pressName(event);
        if (pressKey) delete swallowed[pressKey];
        if (key === 'Escape' || key === 'Esc') {
            event.preventDefault();
            if (event.repeat) return;
            // In the roster's find field with text in it, Escape clears the text and stays; otherwise the first
            // Escape leaves the field and the next one goes back.
            if (typing && active.classList && active.classList.contains('findbox') && active.value !== '') { app().dispatch('roster.clear', null); return; }
            // The empty find field gives the focus to the ledger's start (the selected row), never to body, so the
            // control remembered for the layer is no longer the text field.
            if (typing && active.classList && active.classList.contains('findbox')) {
                const start = primary('ledger');
                if (start) focus(start); else active.blur();
                return;
            }
            if (typing) { active.blur(); return; }
            runBack();
            return;
        }
        if (event.code === 'KeyR' && event.ctrlKey) {
            // Never the page's own reload. In a field, or in Favorites mode, Ctrl + R does nothing; in popup
            // mode the ledger is not open, so it is not a close either (no popup or queue dropped).
            event.preventDefault();
            if (!isCloseChord(event, app().state, typing) || event.repeat) return;
            if (app().view.mode === 'ledger') press('roster', 'close');
            // A popup alone: the game's hotkey is muted while the page holds the keys, so the page asks to open.
            else if (app().view.mode === 'popup' && app().view.popup) RM.bridge.send('romantasyOpen');
            return;
        }
        if (top === 'popup') {
            if (pressKey) {
                handled(event, pressKey);
                if (!event.repeat) press('popup', 'popup.dismiss');
            } else if (key === 'Tab' || ARROWS[key]) {
                // Up and Down scroll the Unlocked list, if there is one, while Continue keeps the focus.
                event.preventDefault();
                const dir = ARROWS[key], list = dir === 'up' || dir === 'down' ? unlockList() : null;
                if (list) nudgeBody(list, dir);
                if (!list || !within(rootsOf('popup'), active)) focus(primary('popup'));
            }
            return;
        }
        if (key === 'Tab') {
            // Tab cycles the top layer's controls only.
            event.preventDefault();
            const roots = rootsOf(top), list = controls(roots), p = place(list, active, roots), delta = event.shiftKey ? -1 : 1;
            if (list.length) focus(list[p === -1 ? (delta > 0 ? 0 : list.length - 1) : (beside(p, delta) + list.length) % list.length]);
            return;
        }
        if (typing) {
            // A field keeps its letters, Space, arrows, Home, End and Backspace; Enter in the password field submits.
            if (key === 'Enter' && top === 'modal' && within(rootsOf('modal'), active)) {
                handled(event, pressKey);
                if (!event.repeat) press('modal', 'modal.unlock');
            }
            return;
        }
        if (pressKey) {
            handled(event, pressKey);   // the page acts, not the button's own key handling: one action per press
            if (!event.repeat) activate(top, key, active);
            return;
        }
        const dir = ARROWS[key];
        if (dir) { event.preventDefault(); arrow(top, dir, active); }
    }

    // ---------- one action per press ----------
    // Magelight fires a key as separate Ultralight events: RawKeyDown (keydown), then Char (keypress) from the
    // window's WM_CHAR, then KeyUp. A preventDefault on the keydown does not cancel the keypress that follows,
    // and WebKit's button clicks itself on keypress Enter and keyup Space. So once the keydown is handled here,
    // that key's keypress and keyup are cancelled in the capture phase on window, before any element (also one
    // that only gained focus from the keydown's own action) sees them.
    function pressName(event) {
        const k = event.key, code = event.keyCode || event.charCode || event.which;
        if (k === 'Enter' || (!k && code === 13)) return 'Enter';
        if (k === ' ' || k === 'Spacebar' || (!k && code === 32)) return ' ';
        return null;
    }
    function handled(event, name) {
        event.preventDefault();
        swallowed[name] = true;
    }
    function swallow(event) {
        const name = pressName(event);
        if (!name || !swallowed[name]) return;
        event.preventDefault();
        if (event.stopImmediatePropagation) event.stopImmediatePropagation();
        else if (event.stopPropagation) event.stopPropagation();
        if (event.type === 'keyup') delete swallowed[name];
    }

    // ---------- the last device used ----------
    const device = () => root.getAttribute('data-ml-input') === 'gamepad' ? 'gamepad' : 'keyboard';
    function useKeyboard() {
        root.classList.add('keys');
        if (device() === 'gamepad') { root.setAttribute('data-ml-input', 'keyboardMouse'); paintLegend(); }
    }
    function onMouseMove() { if (device() === 'gamepad') { root.setAttribute('data-ml-input', 'keyboardMouse'); paintLegend(); } }
    function onMouseDown() { root.classList.remove('keys'); }

    function paintLegend() {
        const el = byId('legend');
        if (!el) return;
        const view = app().view, esc = RM.format.esc;
        const html = topLayer() ? legend(view, device()).map(x => '<span class="hint"><b class="key">' + esc(x.prompt) + '</b><span class="label">'
            + esc(x.label) + '</span></span>').join('') : '';
        if (html === legendHtml) return;
        el.innerHTML = html;
        legendHtml = html;
    }

    // Up and Down from the pad (a press or its repeat), or null for any other action.
    const padDir = e => (e.action === 'up' || e.action === 'down') && (e.phase === 'press' || e.phase === 'repeat') ? e.action : null;
    // Up or Down while the helper is on a scrolling body scrolls it, until its end; then the helper moves on as usual.
    function scrollMarked(name, e) {
        const dir = padDir(e);
        if (!dir) return false;
        const sel = rootsOf(name).map(r => r.querySelector('[data-ml-focused]')).find(Boolean);
        return isBody(sel) && nudgeBody(sel, dir);
    }
    // Left or Right while the helper is on an option moves inside its choice row, as the keys do; the helper's own
    // move would leave the row, since the row's other options are not among its candidates.
    function stepMarked(name, e) {
        if ((e.action !== 'left' && e.action !== 'right') || (e.phase !== 'press' && e.phase !== 'repeat')) return false;
        const sel = rootsOf(name).map(r => r.querySelector('[data-ml-focused]')).find(Boolean);
        return !!sel && optionStep(sel, e.action);
    }
    // Up or Down while the helper is on a control of the settings or journal pane walks the list the keys walk, where a
    // choice row is one stop. The helper's own move goes by position and passes over a row whose checked option sits at
    // its right end, nearer in line to the next full-width row.
    function walkPane(e) {
        const dir = padDir(e), pane = byId('pane');
        if (!dir || !pane) return false;
        const sel = pane.querySelector('[data-ml-focused]');
        if (!sel) return false;
        step(controls([pane]), sel, dir === 'down' ? 1 : -1, [pane]);
        return true;
    }
    // The popup's Unlocked list, when it has one.
    function unlockList() { const p = byId('popup'); return p ? p.querySelector('.scroller') : null; }

    // ---------- gamepad: one navigation scope per open layer ----------
    // The stack, bottom first: the ledger or the settings pane (roster and pane), then the takeover,
    // the modal and the popup when open. The helper answers the last scope attached, so a change low
    // in the stack (the pane switching) re-attaches it and everything above it, in order.
    function scopeOptions(name) {
        const onBack = () => { runBack(); return true; };
        if (name === 'ledger' || name === 'settings' || name === 'journal') {
            // The roster's find field is no pad stop: a gamepad cannot type.
            return { root: byId('app'), onBack, initialFocus: () => primary(name), getCandidates: () => controls(rootsOf(name)).filter(el => !isTyping(el)),
                onAction: e => scrollMarked(name, e) || stepMarked(name, e) || (name !== 'ledger' && walkPane(e)) };
        }
        const options = { root: byId(name), onBack, initialFocus: () => primary(name), onAction: e => scrollMarked(name, e) };
        if (name === 'popup') {
            // Up and Down scroll the Unlocked list while Continue keeps the focus, as the keys do.
            options.onAction = e => {
                const dir = padDir(e), list = dir ? unlockList() : null;
                if (!list) return false;
                nudgeBody(list, dir);
                return true;
            };
        }
        if (name === 'takeover') {
            // The pad's stops are the keys' stops: a summary column with nothing to scroll is passed over. (The helper then
            // checks its selection against this list on every recovery instead of trusting it; it restores as before.)
            options.getCandidates = () => controls(rootsOf('takeover'));
            // A on a deed row stamps it, as Enter does.
            options.onAction = e => {
                if (scrollMarked(name, e)) return true;
                if (e.action !== 'accept' || e.phase !== 'press') return false;
                const sheet = byId('takeover'), sel = sheet && sheet.querySelector('[data-ml-focused]');
                const deed = sel && sel.getAttribute('data-deed');
                if (deed === null || deed === undefined || !sel) return false;
                RM.enrollment.stampNext(deed);
                return true;
            };
        }
        return options;
    }
    function syncScopes(view) {
        if (!helper) return;
        // In popup mode the ledger is not drawn: no scope over it, so A between two popups acts on nothing.
        const wanted = view.mode === 'popup' ? (view.popup ? ['popup'] : [])
            : view.mode !== 'ledger' ? [] : [view.pane === 'settings' || view.pane === 'journal' ? view.pane : 'ledger']
                .concat(view.takeover ? ['takeover'] : [], view.modal ? ['modal'] : [], view.popup ? ['popup'] : []);
        let keep = 0;
        while (keep < scopes.length && keep < wanted.length && scopes[keep].name === wanted[keep]) keep++;
        while (scopes.length > keep) scopes.pop().dispose();
        for (const name of wanted.slice(keep)) {
            const options = scopeOptions(name);
            if (options.root) scopes.push({ name, dispose: helper.attachNavigation(options) });
        }
    }

    // Every focus inside the top layer is noted for that layer, so a layer above it (which focuses its own
    // button while it paints, before render() runs here) cannot hide where the player was.
    function onFocusIn(event) {
        const top = topLayer(), target = event.target;
        if (top && within(rootsOf(top), target)) remembered[top] = target;
        // Magelight's gamepad helper focuses, then scrolls the control into view itself: the end-scroll waits
        // until that is done. Only while the pad is the device in use (a mouse move hands it back), so a
        // pointer's focus never moves the list.
        if (top === 'settings' && device() === 'gamepad') {
            Promise.resolve().then(() => { if (document.activeElement === target && topLayer() === 'settings') reachEnds(target); });
        }
    }

    // The settings list holds text no focus stops on (Health, first or at the foot): a keyboard or pad move onto
    // the list's first control scrolls it to its very start, onto its last control to its very end, so that
    // text shows whenever focus is at that end. Runs after the control is brought into view; never for a click.
    function reachEnds(target) {
        const pane = byId('pane'), list = pane && pane.querySelector('.setlist');
        if (!list || !within([list], target)) return;
        const stops = controls([list]);
        if (target === stops[0]) list.scrollTop = 0;
        else if (target === stops[stops.length - 1]) list.scrollTop = list.scrollHeight;
    }

    // A layer opening over another takes the keyboard's focus off the one below. A layer closing gives the focus
    // back to the control the player last had in the layer below, or to that layer's primary control; the
    // ledger opening starts on its primary control (the ring shows only under html.keys).
    function onLayerChange(from, to) {
        const order = ['ledger', 'settings', 'journal', 'takeover', 'modal', 'popup'];
        const active = document.activeElement;
        if (!to) return;
        if (!from) {
            // Opening from hidden: the page was never reloaded, so the last session's focus (Close, say, if the
            // player closed with a click) means nothing now. Start on the layer's primary control; if the first
            // state then repaints it, keepFocus() follows it to the new element.
            for (const name of Object.keys(remembered)) delete remembered[name];
            const first = primary(to);
            if (first) focus(first);
            return;
        }
        if (order.indexOf(to) > order.indexOf(from)) {
            // The journal opens from a button its own paint removed: it starts on Back.
            if (to === 'journal' && !within(rootsOf(to), active)) { const back = primary(to); if (back) focus(back); return; }
            if (active && active.blur && !within(rootsOf(to), active)) active.blur();
            return;
        }
        if (!within(rootsOf(to), active)) refocus(to);
    }

    // Focus back into a layer: the control last focused there, or the same control repainted (found by its
    // data-ml-id), or the layer's primary control. A body left with nothing to scroll gives way to the next body
    // that scrolls, or else the one before it.
    function refocus(name) {
        const roots = rootsOf(name), was = remembered[name];
        if (was && was.isConnected && within(roots, was) && stop(was)) { focus(was); return; }
        const id = was && was.getAttribute ? was.getAttribute('data-ml-id') : null;
        const target = (id === null ? null : again(roots, id)) || primary(name);
        if (target) focus(target);
    }
    function again(roots, id) {
        const all = liveIn(roots), hit = all.find(el => el.getAttribute('data-ml-id') === id);
        if (!hit || stop(hit)) return hit || null;
        const bodies = all.filter(isBody), i = bodies.indexOf(hit);
        return bodies.slice(i + 1).find(stop) || bodies.slice(0, i).reverse().find(stop) || null;
    }

    // A repaint (a state refresh: points ticking, a new candidate) can detach the focused control and drop the
    // focus to body. Focus that is still inside the top layer, a text field included, is never moved.
    function keepFocus(top) {
        if (!within(rootsOf(top), document.activeElement)) refocus(top);
    }

    // The Ledger size the last render saw: a new size re-lays every list, so the control in use is brought back
    // into view inside its own scroller (a size chosen at the foot of Settings would otherwise leave its row hidden).
    let shownSize = null;

    function render(state, view) {
        const top = topLayer();
        if (top !== shownLayer) { onLayerChange(shownLayer, top); shownLayer = top; }
        else if (top) keepFocus(top);
        const size = root.getAttribute('data-size');
        if (size !== shownSize) {
            const was = shownSize, active = document.activeElement;
            shownSize = size;
            if (was !== null && top && within(rootsOf(top), active)) reveal(active);
        }
        syncScopes(view);
        paintLegend();
    }

    // The key listener once; the gamepad helper as soon as it exists (dev/mock.js calls this
    // again when its stand-in helper has loaded).
    function attach() {
        if (!listening) {
            listening = true;
            document.addEventListener('keydown', onKeyDown);
            document.addEventListener('mousemove', onMouseMove);
            document.addEventListener('mousedown', onMouseDown);
            document.addEventListener('focusin', onFocusIn);
            window.addEventListener('keypress', swallow, true);
            window.addEventListener('keyup', swallow, true);
        }
        if (helper || !RM.bridge.gamepad) return;
        helper = RM.bridge.gamepad();
        if (!helper) return;
        promptOf = action => { const p = helper.getPrompt(action); return p && p.label ? p.label : ''; };
        helper.onStateChange(() => paintLegend());
        if (RM.app) syncScopes(RM.app.view);
    }

    // A mode change repaints nothing by itself: the layer, the scopes and the legend follow it here.
    function init(appApi) {
        appApi.on('mode', () => render(appApi.state, appApi.view));
        attach();
    }

    Object.assign(api, { attach });
    (RM.parts = RM.parts || []).push({ name: 'pad', render, init });
})();
