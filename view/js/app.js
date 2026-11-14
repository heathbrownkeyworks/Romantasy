// The page's spine: holds the latest state and the view, runs the modes, and
// renders every page part in the order the parts registered. Loads last.
//
// A part registers itself when its script loads. Parts load before this file, so
// they queue on RM.parts (the one list app.js renders from):
//     (window.RM.parts = window.RM.parts || []).push({ name: 'ledger', render(state, view) { ... } });
// RM.app.register(name, render) pushes to the same list, for code that runs after boot.
// RM.app.render() calls each part's render(state, view) in list order.
// A part may also carry actions: { name: fn(button, event) }; a click on a button
// with data-action="name" calls the first part's actions[name] (RM.app.activate(button);
// RM.app.dispatch(name, button) runs an action without the click sound).
// A part may also carry init(app): called once at boot, before the first state, so the
// part can subscribe to page events (parts load before RM.app exists).
// Page-internal events: RM.app.on(name, fn) returns an unsubscribe function;
// RM.app.emit(name, payload) calls fn(payload) for every listener of name.
// The app itself emits 'state' (the normalized state) and 'mode' (the new mode).
(function () {
    'use strict';
    const MODES = ['ledger', 'popup', 'hidden'];
    const PETALS = 14;
    // The comfort settings a romantasyMode message may carry (the same names and values as the state's).
    const COMFORT_KEYS = ['ledgerSize', 'highContrast', 'reduceMotion'];
    // How long __uimode may stay off before the host's blink counts as a real close.
    const RELEASE_MS = 250;

    function createEvents() {
        const listeners = new Map();
        return {
            on(name, fn) {
                if (!listeners.has(name)) listeners.set(name, []);
                listeners.get(name).push(fn);
                return () => { const list = listeners.get(name), i = list.indexOf(fn); if (i >= 0) list.splice(i, 1); };
            },
            emit(name, payload) {
                for (const fn of (listeners.get(name) || []).slice()) fn(payload);
            }
        };
    }

    const api = { MODES, createEvents };
    if (typeof module === 'object' && module.exports) { module.exports = api; return; }
    window.RM = window.RM || {};

    const RM = window.RM, root = document.documentElement;
    const events = createEvents();
    const parts = RM.parts = RM.parts || [];
    let booted = false, readyDelivered = false, releaseTimer = 0, modeBeforeRelease = null;

    const app = window.RM.app = {
        MODES,
        state: RM.state.normalize({}),
        view: RM.state.createView(),
        on: events.on,
        emit: events.emit,

        register(name, render) {
            parts.push({ name, render });
        },

        render() {
            for (const part of parts) {
                try { part.render(app.state, app.view); }
                catch (error) { console.error('Romantasy: ' + part.name + ' failed to render', error); }
            }
        },

        setMode(mode) {
            if (!MODES.includes(mode)) return;
            app.view.mode = mode;
            root.setAttribute('data-mode', mode);
            root.classList.toggle('asleep', mode === 'hidden');
            events.emit('mode', mode);
        },

        // Tells the plugin the page is listening, once a host is attached. dev/mock.js
        // calls it again after it attaches its stand-in host.
        connect() {
            if (booted && !readyDelivered) readyDelivered = RM.bridge.send('romantasyReady');
            return readyDelivered;
        },

        // A __uimode release is still deciding whether the host's off was a blink (pad.js asks).
        releasePending: () => releaseTimer !== 0
    };

    // The roster's rows as roster.js arranges them (a state keeps the pinned places), and the selection on them.
    function settleRoster() {
        const previous = app.view.selected, previousIndex = app.view.selectedIndex;
        RM.roster.settle(app.state, app.view);
        const rows = RM.roster.shownRows(app.state, app.view);
        app.view.selected = RM.state.reconcileSelection(previous, previousIndex, rows);
        const index = rows.findIndex(row => RM.state.rowIdentity(row) === app.view.selected);
        app.view.selectedIndex = index < 0 ? 0 : index;
    }

    // The comfort settings on the root, from every state and every mode message that carries them: data-size always, data-contrast and data-motion only while on
    // (the stylesheets answer them). A new size fits the stage again, once; nothing else re-lays the page.
    let sizeFitted = 'standard';
    function applyComfort(state) {
        root.setAttribute('data-size', state.ledgerSize);
        if (state.highContrast) root.setAttribute('data-contrast', 'high'); else root.removeAttribute('data-contrast');
        if (state.reduceMotion) root.setAttribute('data-motion', 'reduced'); else root.removeAttribute('data-motion');
        if (state.ledgerSize !== sizeFitted) {
            sizeFitted = state.ledgerSize;
            RM.fit.apply(sizeFitted);
        }
    }

    // Set when the ledger opens or the page sleeps: the plugin sends the mode before the state, so the first state of
    // an opening is sorted afresh, not pinned to the order made from the last state of the previous session.
    let freshOrder = false;

    function onState(raw) {
        app.state = RM.state.normalize(raw);
        applyComfort(app.state);
        if (freshOrder && app.view.mode === 'ledger') {
            app.view.pinned = null;
            freshOrder = false;
        }
        settleRoster();
        app.render();
        events.emit('state', app.state);
    }

    // Asleep, with nothing of the last session kept: no popup (silently: no Dismissed; its fuse and
    // Shown timer go with it in popup.js), no modal, no takeover, the dossier pane.
    function sleep() {
        app.setMode('hidden');
        app.view.modal = null;
        app.view.pane = 'dossier';
        app.view.takeover = null;
        app.view.popup = null;
        freshOrder = true;
        RM.roster.reset(app.view);
        settleRoster();
        app.render();
    }

    // A mode from the plugin ends any pending release: the host is in UI mode again for that mode,
    // so the old timer must neither drop what the new mode shows nor restore the mode it replaced.
    // "hidden" is the plugin leaving: what a completed release does, at once (after a refused entry
    // no __uimode ever comes to say so).
    function onMode(payload) {
        const mode = payload && payload.mode;
        if (!MODES.includes(mode)) return;
        clearTimeout(releaseTimer);
        releaseTimer = 0;
        modeBeforeRelease = null;
        if (mode === 'hidden') { sleep(); return; }
        // The mode carries the comfort settings, so a popup that comes before any state follows them.
        // A key it leaves out keeps what the page has.
        if (COMFORT_KEYS.some(key => key in payload)) {
            const comfort = {};
            for (const key of COMFORT_KEYS) comfort[key] = key in payload ? payload[key] : app.state[key];
            const normalized = RM.state.normalize(comfort);
            for (const key of COMFORT_KEYS) app.state[key] = normalized[key];
            applyComfort(app.state);
        }
        // The ledger opening: no find text, and the rows sorted afresh (the state that follows is sorted afresh too).
        if (mode === 'ledger' && app.view.mode !== 'ledger') {
            freshOrder = true;
            RM.roster.reset(app.view);
            settleRoster();
            app.render();
        }
        app.setMode(mode);
        // The plugin enters popup mode only while the ledger is closed: drop the ledger's open sheets
        // (the release this replaced would have), so they do not come back when it next opens.
        if (mode === 'popup') {
            app.view.modal = null;
            app.view.pane = 'dossier';
            app.view.takeover = null;
            app.render();
        }
    }

    // The host can blink __uimode off and on while it swaps the popup for the ledger:
    // go dark at once, but drop the open sheets only if it is still off a moment later.
    // UI mode coming on while the page believes it is hidden, with no release to undo, means the page
    // missed the plugin's mode: asking again as at boot makes the plugin send its mode and state.
    function onUiMode(focused) {
        if (focused) {
            if (releaseTimer) {
                clearTimeout(releaseTimer);
                releaseTimer = 0;
                if (app.view.mode === 'hidden' && modeBeforeRelease) app.setMode(modeBeforeRelease);
            } else if (app.view.mode === 'hidden') {
                RM.bridge.send('romantasyReady');
            }
            return;
        }
        if (app.view.mode !== 'hidden') modeBeforeRelease = app.view.mode;
        app.setMode('hidden');
        clearTimeout(releaseTimer);
        releaseTimer = setTimeout(() => {
            releaseTimer = 0;
            modeBeforeRelease = null;
            if (RM.bridge.focused) return;
            sleep();
        }, RELEASE_MS);
    }

    // A control is a button with data-action, handled by the first part whose actions map names it.
    function dispatch(name, el, event) {
        for (const part of parts) {
            if (part.actions && typeof part.actions[name] === 'function') { part.actions[name](el, event); return; }
        }
    }

    // What a click on a control does (pad.js runs Enter, Space and Escape through it too):
    // every hit plays the click sound, then its action runs.
    function activate(hit, event) {
        if (!hit || hit.disabled) return;
        RM.bridge.sound('click');
        const name = hit.getAttribute('data-action');
        if (name === null) return;
        dispatch(name, hit, event);
    }

    // The page's one click listener.
    function onClick(event) {
        activate(event.target.closest && event.target.closest('[data-action],[data-identity]'), event);
    }
    app.activate = activate;
    app.dispatch = dispatch;

    function onSelect(identity) {
        const rows = RM.roster.shownRows(app.state, app.view);
        const index = rows.findIndex(row => RM.state.rowIdentity(row) === identity);
        if (index < 0) return;
        app.view.selected = identity;
        app.view.selectedIndex = index;
        app.view.pane = 'dossier';
        app.render();
    }

    // The page is a game screen and never scrolls: only a card's own body does. Overflow hidden still lets a
    // script scroll a box (the host's gamepad helper bringing a control into view, say), so any scroll of the
    // page, the stage or its layers is put back at once. Scroll events do not bubble; the capture phase sees every one.
    function pinPage() {
        const boxes = [root, document.body].concat(['app', 'pane', 'roster', 'takeover', 'modal', 'popup'].map(id => document.getElementById(id)));
        for (const el of boxes) {
            if (el && (el.scrollTop || el.scrollLeft)) { el.scrollTop = 0; el.scrollLeft = 0; }
        }
        if (window.scrollX || window.scrollY) window.scrollTo(0, 0);
    }

    function scatterPetals() {
        const host = document.getElementById('petals');
        if (!host) return;
        const r = Math.random;
        let html = '';
        for (let i = 0; i < PETALS; i++) {
            html += '<i class="petal" style="--x:' + (r() * 100).toFixed(1) + '%;--d:' + (6 + r() * 10).toFixed(1)
                + 's;--delay:' + (-r() * 30).toFixed(1) + 's;--s:' + (.5 + r()).toFixed(2) + '"></i>';
        }
        host.innerHTML = html;
    }

    function boot() {
        if (booted) return;
        booted = true;
        if (!(window.CSS && CSS.supports && CSS.supports('background-image', 'repeating-conic-gradient(#000 0deg, #fff 10deg)'))) {
            root.classList.add('no-conic');
        }
        RM.fit.apply();   // fit.js listens for resize itself; its own DOMContentLoaded may have passed
        root.setAttribute('data-mode', app.view.mode);
        root.setAttribute('data-size', sizeFitted);
        scatterPetals();
        document.getElementById('app').addEventListener('click', onClick);
        document.addEventListener('scroll', pinPage, true);
        events.on('select', onSelect);
        events.on('refresh', () => RM.bridge.send('romantasyRefresh'));
        events.on('close', () => RM.bridge.send('romantasyClose'));
        for (const part of parts) {
            if (typeof part.init !== 'function') continue;
            try { part.init(app); }
            catch (error) { console.error('Romantasy: ' + part.name + ' failed to start', error); }
        }
        RM.bridge.on('romantasyState', onState);
        RM.bridge.on('romantasyMode', onMode);
        RM.bridge.on('__uimode', onUiMode);
        app.connect();
    }

    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
    else boot();
})();
