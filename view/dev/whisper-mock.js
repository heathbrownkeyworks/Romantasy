// Development stand-in for the plugin on the whisper page. Loaded only with whisper.html?mock; never shipped.
// It installs a stand-in host if the real one is absent, attaches the page to it, and plays whispers as
// the plugin would: one at a time, the next PAUSE_MS after the page answers romantasyWhisperDone for the
// one on screen. It cycles the samples (a gain, a loss and a plain one first, then the other two icons).
// Every answer is logged.
//
// Query switches (after ?mock):
//   &long            every name and line is far too long for the card (they end in an ellipsis)
//   &bg=light        a bright snowfield behind the card instead of the night sky
// From the console, __whisperMock.send({ name, line, tone, icon }) sends one now (with the next seq).
(function () {
    'use strict';
    const q = location.search;
    const flag = name => new RegExp('[?&]' + name + '(?=&|=|$)').test(q);
    const param = name => { const m = new RegExp('[?&]' + name + '=([^&]*)').exec(q); return m ? decodeURIComponent(m[1]) : ''; };
    const PAUSE_MS = 800;

    const SAMPLES = [
        { name: 'Aelira', line: 'Dungeons Cleared · +10', tone: 'gain', icon: 'star' },
        { name: 'Serana', line: 'Murders · −20', tone: 'loss', icon: 'heartbroken' },
        { name: 'Inalion', line: 'Has been away from you for days', tone: 'plain', icon: 'neutral' },
        { name: 'Ambellina', line: '3 deeds · +12', tone: 'gain', icon: 'heart' },
        { name: 'Lydia', line: 'Pickpocketing · −5', tone: 'loss', icon: 'dislikes' }
    ];
    const LONG_NAME = 'Aelira the Rose of the Reach, Bard of the Winking Skeever and Keeper of Songs';
    const LONG_LINE = 'A deed whose label is far longer than any card could ever hope to show on one line · +10';
    const samples = SAMPLES.map(s => flag('long') ? Object.assign({}, s, { name: LONG_NAME, line: LONG_LINE }) : s);

    // A backdrop for the browser only: the real page's body is transparent over the game.
    document.documentElement.classList.add('mock');
    const style = document.createElement('style');
    style.textContent = 'html.mock body{padding:40px;background:' + (param('bg') === 'light'
        ? 'linear-gradient(160deg,#f4f7fb,#c9d4df)' : 'linear-gradient(160deg,#1d2733,#06080c)') + '}';
    document.head.appendChild(style);

    const subs = new Map();
    let seq = 0, turn = 0;
    const mock = { log: [], samples, PAUSE_MS, send };
    window.__whisperMock = mock;
    if (!window.magelight || typeof window.magelight.on !== 'function') {
        window.magelight = {
            send(name, json) { let payload = {}; try { payload = json ? JSON.parse(json) : {}; } catch (error) { payload = json; } handle(name, payload); },
            on(channel, fn) { if (!subs.has(channel)) subs.set(channel, []); subs.get(channel).push(fn); },
            off() {}
        };
    }

    function send(whisper) {
        const payload = Object.assign({}, whisper, { seq: ++seq });
        for (const fn of (subs.get('romantasyWhisper') || []).slice()) fn(JSON.stringify(payload));
        return payload.seq;
    }

    function next() { send(samples[turn++ % samples.length]); }

    function handle(name, payload) {
        mock.log.push({ name, payload });
        console.log('mock: ' + name, payload);
        if (name !== 'romantasyWhisperDone') { console.warn('mock: unknown action ' + name); return; }
        if (payload && payload.seq === seq) setTimeout(next, PAUSE_MS);
    }

    const RM = window.RM || {};
    if (RM.whisper) RM.whisper.connect(window.magelight);
    console.log('Romantasy whisper mock ready' + (q.length > 5 ? ': ' + q.slice(1) : ''));
    next();
})();
