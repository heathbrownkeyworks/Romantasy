// The whisper card: a small note on the HUD when a follower's bond changes. It is its own page
// (whisper.html) in a click-through HUD view, separate from the ledger, and it never takes input.
// Dual-use: the pure parts (normalize, cardHtml, createPlayer) are exported to Node for the tests.
//
// Plugin -> page: romantasyWhisper { name, line, tone: 'gain' | 'loss' | 'plain', icon, seq, contrast?, motion? }.
// contrast "high" and motion "reduced" (each optional; anything else is the normal look) go on the page's root as
// data-contrast and data-motion, from every payload, as the ledger's Stronger contrast and Reduce motion do.
// Page -> plugin: romantasyWhisperDone { seq } once that whisper has left the screen, or at once when
// a newer one replaces it while it is up. seq is 0 when the payload's was not a whole number from 1.
//
// The sequence runs on timers, never on animation events (a hidden view may never fire them):
// shown at 0, leaving at HOLD_MS, gone and answered at HOLD_MS + LEAVE_MS. ENTER_MS is the CSS fade in.
//
// This file links itself to the host as bridge.js does for the ledger (window.magelight, or the
// stand-in dev/whisper-mock.js attaches); it does not load bridge.js or share its lists.
(function () {
    'use strict';
    const node = typeof module === 'object' && module.exports;
    const esc = (node ? require('./format.js') : window.RM.format).esc;
    const HOLD_MS = 4000;
    const ENTER_MS = 180;
    const LEAVE_MS = 220;
    const MAX_SEQ = 2147483647;
    const TONES = ['gain', 'loss', 'plain'];
    // The icons the plugin names, each a file under view/icons; any other name shows the fallback.
    const ICONS = ['star', 'heart', 'heartbroken', 'dislikes', 'neutral'];
    const FALLBACK_ICON = 'neutral';

    const text = v => typeof v === 'string' ? v : '';

    function normalize(raw) {
        const w = raw && typeof raw === 'object' && !Array.isArray(raw) ? raw : {};
        return {
            name: text(w.name),
            line: text(w.line),
            tone: TONES.includes(w.tone) ? w.tone : 'plain',
            icon: ICONS.includes(w.icon) ? w.icon : FALLBACK_ICON,
            seq: Number.isInteger(w.seq) && w.seq >= 1 && w.seq <= MAX_SEQ ? w.seq : 0
        };
    }

    // The look a payload asks for: only the exact strings turn a setting on.
    function comfort(raw) {
        const w = raw && typeof raw === 'object' && !Array.isArray(raw) ? raw : {};
        return { contrast: w.contrast === 'high', motion: w.motion === 'reduced' };
    }

    function cardHtml(raw) {
        const w = normalize(raw);
        return '<div class="whisper ' + w.tone + '">'
            + '<img class="icon" src="icons/' + w.icon + '.png" alt="" draggable="false">'
            + '<div class="words"><p class="name">' + esc(w.name) + '</p><p class="line">' + esc(w.line) + '</p></div>'
            + '</div>';
    }

    // One whisper on screen at a time. io: { setTimeout, clearTimeout, paint(html), phase('in' | 'out'),
    // clear(), done(seq) }. Exactly one done per whisper: when its time ends, or at once when replaced.
    function createPlayer(io) {
        let current = null, hold = 0, leave = 0;

        function stop() {
            io.clearTimeout(hold);
            io.clearTimeout(leave);
            hold = leave = 0;
        }

        function finish() {
            const seq = current.seq;
            stop();
            current = null;
            io.clear();
            io.done(seq);
        }

        function show(raw) {
            const w = normalize(raw);
            // Nothing to say: not shown, answered at once; a whisper on screen is left alone.
            if (!w.name && !w.line) { io.done(w.seq); return; }
            // The plugin sent the one on screen again: its time runs on.
            if (current && w.seq > 0 && w.seq === current.seq) return;
            if (current) finish();
            current = w;
            io.paint(cardHtml(w));
            io.phase('in');
            hold = io.setTimeout(() => {
                hold = 0;
                io.phase('out');
                leave = io.setTimeout(() => { leave = 0; finish(); }, LEAVE_MS);
            }, HOLD_MS);
        }

        return { show };
    }

    function parse(raw) {
        if (typeof raw !== 'string') return raw;
        try { return JSON.parse(raw); } catch (error) { return null; }
    }

    const api = { HOLD_MS, ENTER_MS, LEAVE_MS, ICONS, FALLBACK_ICON, normalize, comfort, cardHtml, createPlayer, parse };
    if (node) { module.exports = api; return; }

    /* ---------- DOM and host ---------- */
    let link = null;
    const card = () => document.getElementById('whisper');
    const player = createPlayer({
        setTimeout: (fn, ms) => setTimeout(fn, ms),
        clearTimeout: id => clearTimeout(id),
        paint: html => { const el = card(); if (el) el.innerHTML = html; },
        phase: name => { const el = card(); if (el) el.className = name; },
        clear: () => { const el = card(); if (el) { el.className = ''; el.innerHTML = ''; } },
        // A send that throws must not stop the caller: during a replace it would leave the new whisper unpainted.
        done: seq => {
            if (!link || typeof link.send !== 'function') return;
            try { link.send('romantasyWhisperDone', JSON.stringify({ seq })); } catch (error) { console.warn('romantasyWhisperDone failed: ' + error); }
        }
    });

    // Attach to the host once: in game window.magelight, in a browser the harness's stand-in.
    function connect(host) {
        if (link || !host || typeof host.on !== 'function') return false;
        link = host;
        host.on('romantasyWhisper', raw => {
            const payload = parse(raw), look = comfort(payload), root = document.documentElement;
            if (look.contrast) root.setAttribute('data-contrast', 'high'); else root.removeAttribute('data-contrast');
            if (look.motion) root.setAttribute('data-motion', 'reduced'); else root.removeAttribute('data-motion');
            player.show(payload);
        });
        return true;
    }

    api.connect = connect;
    window.RM = window.RM || {};
    window.RM.whisper = api;
    if (window.magelight && typeof window.magelight.on === 'function') connect(window.magelight);
})();
