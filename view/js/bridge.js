// The bridge between the page and the plugin: a store of the latest value of
// every topic the plugin publishes, and send() for the page's actions. In game
// the host is window.magelight; in a browser dev/mock.js installs a stand-in
// and attaches it. This is the only file that names window.magelight.
// Dual-use: the pure parts are exported to Node for the tests.
(function () {
    'use strict';
    // Plugin -> page.
    const TOPICS = ['romantasyState', 'romantasyMode', 'romantasyLevelChange', 'romantasyJournal'];
    // Page -> plugin.
    const ACTIONS = [
        'romantasyReady', 'romantasyOpen', 'romantasyClose', 'romantasyRefresh', 'romantasySettings', 'romantasyBond',
        'romantasyDevUnlock', 'romantasyDevLock', 'romantasyDebug', 'romantasyPopupShown', 'romantasyPopupDismissed',
        'romantasyJournalOpen'
    ];
    // The keys each action's payload may carry.
    const PAYLOAD_KEYS = {
        romantasySettings: ['openWithFavorites', 'showGainModals', 'showLossModals', 'showAwayFollowers', 'detailedLog', 'pace', 'tierMoment', 'whispers', 'whisperCorner', 'rosterOrder', 'ledgerSize', 'highContrast', 'reduceMotion', 'skyrimNetBridge', 'skyrimNetConversation', 'ostimMoments', 'ostimHold'],
        romantasyBond: ['kind', 'identity', 'preferences'],
        romantasyDevUnlock: ['password'],
        romantasyDebug: ['points', 'stat', 'delta'],
        romantasyPopupShown: ['seq'],
        romantasyPopupDismissed: ['seq'],
        romantasyJournalOpen: ['identity']
    };

    function createStore() {
        const values = new Map(), versions = new Map(), listeners = new Map(), any = [];
        return {
            publish(topic, value) {
                values.set(topic, value);
                versions.set(topic, (versions.get(topic) || 0) + 1);
                for (const fn of listeners.get(topic) || []) fn(value, topic);
                for (const fn of any) fn(topic, value);
            },
            // A late subscriber gets the latest value at once; '*' hears every publish after its own.
            on(topic, fn) {
                if (topic === '*') { any.push(fn); return; }
                if (!listeners.has(topic)) listeners.set(topic, []);
                listeners.get(topic).push(fn);
                if (values.has(topic)) fn(values.get(topic), topic);
            },
            get: topic => values.get(topic),
            has: topic => values.has(topic),
            version: topic => versions.get(topic) || 0
        };
    }

    function parse(raw) {
        if (typeof raw !== 'string') return raw;
        if (raw === '') return null;
        try { return JSON.parse(raw); } catch (error) { return null; }
    }

    if (typeof module === 'object' && module.exports) {
        module.exports = { createStore, TOPICS, ACTIONS, PAYLOAD_KEYS, parse };
        return;
    }

    const store = createStore();
    const bridge = Object.assign({}, store, {
        TOPICS, ACTIONS, PAYLOAD_KEYS, parse, createStore,
        host: null,
        focused: false,
        attach(host) {
            bridge.host = host;
            for (const topic of TOPICS) host.on(topic, raw => store.publish(topic, parse(raw)));
            host.on('__uimode', raw => {
                bridge.focused = raw === '1' || raw === 1 || raw === true;
                store.publish('__uimode', bridge.focused);
            });
        },
        send(name, payload) {
            if (!ACTIONS.includes(name)) throw new Error('Romantasy: unknown action ' + name);
            if (!bridge.host || typeof bridge.host.send !== 'function') return false;
            bridge.host.send(name, JSON.stringify(payload === undefined ? {} : payload));
            return true;
        },
        sound(name) {
            if (bridge.host && typeof bridge.host.sound === 'function') bridge.host.sound(name);
        },
        // Magelight's gamepad helper (host 1.6.0), or null: without the helper, or when the host
        // says the gamepad layer is off.
        gamepad() {
            const pad = window.magelight && window.magelight.gamepad;
            const info = window.__MAGELIGHT__;
            return pad && typeof pad.attachNavigation === 'function' && info && info.capabilities && info.capabilities.gamepad ? pad : null;
        }
    });
    window.RM = window.RM || {};
    window.RM.bridge = bridge;
    if (window.magelight && typeof window.magelight.on === 'function') bridge.attach(window.magelight);
})();
