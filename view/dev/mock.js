// Development stand-in for the plugin. Loaded only with ?mock; never shipped.
// It installs a stand-in host if the real one is absent, attaches the bridge to
// it, and answers each page action the way RomantasyUI would, on a working copy
// of the fixture so the page can be exercised end to end. Every action is logged.
//
// Query switches (after ?mock):
//   &empty            start from the empty fixture (no bonds)
//   &mode=popup       the plugin opens in popup mode instead of the ledger
//   &popup=gain|loss  a level change arrives right after the first state
// From the console, __romantasyMock.popup('gain' | 'loss') sends another level change (each with
// the next seq, as RomantasyUI numbers its queue); Shown and Dismissed are logged with their seq.
//   &health=warn|error|note   the health report carries items of those levels (plain ?mock is healthy, no items)
//   &heavy            the first bond and the roster carry so much that every scroller in the ledger has to scroll,
//                     and every journal is the fullest the plugin keeps
//   &pace=slow|fast   start at that Relationship pace; under Slow burn the first bond has a gate ahead
//   &skyrimnet        SkyrimNet was found: Settings shows its two rows (both on)
//   &ostim            OStim was found: Settings shows its two rows (both on), and the first bond's journal
//                     carries "A first moment together"
//   &size=large|largest   start at that Ledger size; &contrast and &motion start with Stronger contrast and Reduce motion on
//   &fail             every bond action (enroll, replace, reset, remove) is refused, and every journal is not found
//   &pad              a gamepad in use: the dev copy of Magelight's helper (dev/magelight-gamepad.js) loads after
//                     this file and __romantasyMock.pad feeds it the host's packets, e.g. pad.press('down'),
//                     pad.press('accept'), pad.press('cancel'); pad.keyboard() hands the page back to the keyboard.
// Developer tools unlock with the password "mock".
(function () {
    'use strict';
    const q = location.search;
    const flag = name => new RegExp('[?&]' + name + '(?=&|=|$)').test(q);
    const param = name => { const m = new RegExp('[?&]' + name + '=([^&]*)').exec(q); return m ? decodeURIComponent(m[1]) : ''; };
    const clone = v => JSON.parse(JSON.stringify(v));

    const state = clone((flag('empty') ? window.ROMANTASY_FIXTURE_EMPTY : window.ROMANTASY_FIXTURE) || {});
    state.romances = state.romances || [];
    state.enrollmentCandidates = state.enrollmentCandidates || [];
    state.preferenceOptions = state.preferenceOptions || [];
    state.detailedLog = state.detailedLog === true;
    state.pace = param('pace') === 'slow' || param('pace') === 'fast' ? param('pace') : 'standard';
    state.tierMoment = 'calm';
    state.whispers = true;
    state.whisperCorner = 'topRight';
    state.rosterOrder = 'closest';
    state.ledgerSize = param('size') === 'large' || param('size') === 'largest' ? param('size') : 'standard';
    state.highContrast = flag('contrast');
    state.reduceMotion = flag('motion');
    state.skyrimNet = { found: flag('skyrimnet'), bridge: true, conversation: true };
    state.ostim = { found: flag('ostim'), moments: true, hold: true };
    state.health = healthReport(param('health'));
    richer(state.romances);
    const fail = flag('fail');
    // &pad: the gamepad layer is on, as the host's capabilities would say.
    if (flag('pad')) window.__MAGELIGHT__ = Object.assign({}, window.__MAGELIGHT__, { capabilities: { gamepad: 1 } });
    // The health report, as the plugin builds it: items worst first; the status is the worst level, or ok.
    function healthReport(kind) {
        const ITEMS = {
            conditions: ['error', 'Dialogue conditions are off', 'This game version is not supported by the dialogue bridge. Romance dialogue may not unlock.'],
            profiles: ['warn', '3 personality file(s) could not be used', 'Some Long File Name.toml: schema_version must be integer 1, 2 or 3\n'
                + 'Another Follower Personality.toml: schema_version must be integer 1, 2 or 3\nThird Companion With A Long Name.toml: schema_version must be integer 1, 2 or 3'],
            power: ['warn', 'The Romantasy power is missing', 'Opening the ledger from the Favorites menu will not work.'],
            orphans: ['note', 'Keeping 2 bond(s) for followers whose mods are not loaded', 'They return when the mods do. Nothing is lost.']
        };
        const ids = { warn: ['profiles', 'orphans'], error: ['conditions', 'power', 'orphans'], note: ['orphans'] }[kind] || [];
        const items = ids.map(id => ({ id, level: ITEMS[id][0], title: ITEMS[id][1], detail: ITEMS[id][2] }));
        const status = items.some(i => i.level === 'error') ? 'error' : items.some(i => i.level === 'warn') ? 'warn' : 'ok';
        return { status, version: '3.0.1', host: '1.6.2', bonds: state.romances.length, items };
    }
    // Schema 2 personalities, as a plugin with favorites and unlock lines sends them (the fixture stays schema 1):
    // Aelira has two favorites and a pet peeve; Serana (Confidant) has unlock lines at Friend, Lover and Spouse.
    // Every row but the last has a mood (all three in turn); the last has none, as from an older plugin.
    function richer(rows) {
        const MOODS = ['warm', 'settled', 'strained'];
        rows.slice(0, -1).forEach((row, i) => { row.mood = MOODS[i % MOODS.length]; });
        const aelira = rows.find(r => r.name === 'Aelira'), serana = rows.find(r => r.editorID === 'CSV_Serana');
        if (aelira) Object.assign(aelira, { favorites: ['Dungeons Cleared', 'Quests Completed'], petPeeves: ['Murders'] });
        if (serana) serana.unlocks = [
            { level: 3, items: ["She shares her mother's journal"] },
            { level: 5, items: ["A key to Castle Volkihar's garden", "Her mother's ring"] },
            { level: 6, items: ['A wedding at the Temple of Mara'] }
        ];
    }
    const TIERS = [['Stranger', 0], ['Acquaintance', 500], ['Friend', 1000], ['Confidant', 1500], ['Lover', 2000], ['Spouse', 2500]];
    const levelName = points => TIERS.reduce((name, [n, at]) => (points >= at ? n : name), 'Stranger');
    if (flag('heavy')) heavy(state);
    gates();
    changeDays(state.romances);

    // The game day of each recent change, as the plugin will send it with the day words: today is day 120, and bonds
    // changed on the same day changed at different hours (each row its own part of the day), so the Recent order
    // within "Today" shows. Entries run newest first, as the plugin keeps them.
    function changeDays(rows) {
        const TODAY = 120;
        rows.forEach((row, r) => (row.recent || []).forEach((entry, i) => {
            const when = entry.when || '', n = /^(\d+) days ago$/.exec(when);
            const ago = when === 'Today' ? 0 : when === 'Yesterday' ? 1 : n ? Number(n[1]) : null;
            if (ago !== null) entry.day = TODAY - ago + 0.9 - ((r * 3) % 8) / 10 - i / 1000;
        }));
    }

    // Slow burn's gate, as the plugin sends it: under Slow burn the first bond has one ahead for its next tier
    // (Friend 3 days, Confidant 7, Lover 14, Spouse 21) with 1.46 days together so far; no row has one otherwise.
    function gates() {
        const DAYS = { 3: 3, 4: 7, 5: 14, 6: 21 };
        state.romances.forEach((row, i) => {
            const level = TIERS.filter(([, at]) => row.points >= at).length + 1;
            if (i === 0 && state.pace === 'slow' && DAYS[level]) row.gate = { level, name: TIERS[level - 1][0], daysNeeded: DAYS[level], daysTogether: 1.46 };
            else delete row.gate;
        });
    }

    // &heavy: enough of everything that every scroller in the ledger has to scroll. The first bond gets 30 likes and
    // 30 dislikes (3 of each strong), 40 recent changes, 4 long unlock lines for its next tier and a long plugin name
    // (the Personality card's text); the roster is filled to 30 bonds.
    function heavy(st) {
        const first = st.romances[0];
        if (!first) return;
        const labels = st.preferenceOptions.map(o => o.label);
        const label = i => labels[i] || 'Deed ' + (i + 1);
        const range = (from, n) => Array.from({ length: n }, (_, i) => label(from + i));
        const level = TIERS.filter(([, at]) => first.points >= at).length;
        Object.assign(first, {
            likes: range(0, 30), dislikes: range(30, 30),
            favorites: range(0, 3), petPeeves: range(30, 3),
            recent: Array.from({ length: 40 }, (_, i) => ({
                label: label((i * 7) % 60), points: i % 3 === 2 ? -5 : 1 + (i % 4),
                when: i < 4 ? 'Today' : i < 10 ? 'Yesterday' : Math.floor(i / 3) + ' days ago'
            })),
            unlocks: [{ level: level + 1, items: [
                'A private room at the Bannered Mare, kept for you alone',
                "Her grandmother's lute, restrung, and the songs she wrote",
                "A standing seat at the bards' table in the Winking Skeever",
                'The name of the Reach valley where she was born, at last'
            ] }],
            sourcePlugin: 'Aelira - The Rose of the Reach - A Fully Voiced Bard Companion With Custom Dialogue, Romance, Banter, '
                + 'Quests, Camping, Outfits and a Much Longer Name Than Any Plugin Should Ever Have, Kept To Test The Personality Card.esp'
        });
        const base = st.romances.slice();
        for (let i = st.romances.length; i < 30; i++) {
            const copy = clone(base[i % base.length]);
            const id = (0xE0000000 + i).toString(16).toUpperCase();
            Object.assign(copy, { name: copy.name.replace(/\s*<b>$/, '') + ' ' + (i + 1), editorID: '', referenceFormID: id, baseFormID: id });
            st.romances.push(copy);
        }
    }

    document.documentElement.classList.add('mock');
    const style = document.createElement('style');
    style.textContent = 'html.mock[data-mode="popup"] body{background:#3b3a40}';   // stands in for the game behind a popup
    document.head.appendChild(style);

    const subs = new Map();
    let seq = 0;
    const mock = { log: [], sounds: [], state, publish, handle, popup: kind => publish('romantasyLevelChange', { ...levelChange(kind), seq: ++seq }) };
    window.__romantasyMock = mock;
    if (!window.magelight || typeof window.magelight.on !== 'function') {
        window.magelight = {
            send(name, json) { let payload = {}; try { payload = json ? JSON.parse(json) : {}; } catch (error) { payload = json; } handle(name, payload); },
            on(channel, fn) { if (!subs.has(channel)) subs.set(channel, []); subs.get(channel).push(fn); return () => { const list = subs.get(channel), i = list.indexOf(fn); if (i >= 0) list.splice(i, 1); }; },
            off() {},
            sound(name) { mock.sounds.push(name); console.log('mock: sound ' + name); }
        };
    }

    function publish(topic, value) {
        const text = value === undefined || value === null ? '' : typeof value === 'string' ? value : JSON.stringify(value);
        for (const fn of (subs.get(topic) || []).slice()) fn(text);
    }

    let sent = false;
    function sendState(message) {
        if (sent) state.revision = (Number(state.revision) || 0) + 1;
        sent = true;
        state.health.bonds = state.romances.length;
        gates();
        state.message = message;
        publish('romantasyState', state);
    }

    // A journal for one bond, as JournalJson.cpp shapes it: milestones oldest first (the journal began, each tier up
    // to the bond's own reached once, then a fall and the climb back), days oldest first with some of only a gain and
    // some of only a loss, the biggest moments, and the history newest first. With &heavy: the fullest journal the
    // plugin keeps (26 milestones, 60 days, 5 + 5 moments, 30 history rows) with labels at the 120-byte limit.
    function journalOf(row, identity) {
        const full = flag('heavy');
        const level = Math.max(2, TIERS.filter(([, at]) => row.points >= at).length);
        const labels = state.preferenceOptions.map(o => o.label);
        const long = i => ('Deed ' + (i + 1) + ': a long entry that the plugin cut at its limit, written to test how a label wraps inside the journal page and its cards').slice(0, 120);
        const label = i => full ? long(i) : labels[i % Math.max(1, labels.length)] || 'Deed ' + (i + 1);
        const milestones = [{ kind: 'began', level: 1, day: 2 }];
        for (let l = 2; l <= level; l++) milestones.push({ kind: 'reached', level: l, day: 2 + (l - 1) * 4 });
        const falls = full ? 20 : 1;
        for (let i = 0; i < falls; i++) milestones.push({ kind: 'fell', level: Math.max(1, level - 1 - (i % 2)), day: 30 + i * 3 });
        // &heavy needs 26: the climb starts at Stranger, so pad the reached ones up to five with the tiers above.
        for (let l = level + 1; full && milestones.length < 26 && l <= 6; l++) milestones.splice(l - 1, 0, { kind: 'reached', level: l, day: 2 + (l - 1) * 4 });
        // &ostim: the first bond shared a moment on day 7, placed by day as JournalJson.cpp places it.
        if (flag('ostim') && row === state.romances[0]) {
            const at = milestones.findIndex(m => m.day > 7);
            milestones.splice(at < 0 ? milestones.length : at, 0, { kind: 'shared', level: 0, day: 7 });
        }
        const count = full ? 60 : 20;
        const days = Array.from({ length: count }, (_, i) => {
            const gained = i % 7 === 6 ? 0 : 5 + (i * 37) % 60, lost = i % 4 === 3 || i % 7 === 6 ? 4 + (i * 13) % 30 : 0;
            return { day: 2 + i * 2, gained, lost };
        });
        const change = (i, points) => ({ label: label(i), points, day: 2 + ((i * 11) % count) * 2 });
        return {
            identity, found: true, began: 2, daysTogether: full ? 118.5 : 12.4, milestones, days,
            gains: Array.from({ length: full ? 5 : 3 }, (_, i) => change(i, 60 - i * 9)),
            losses: Array.from({ length: full ? 5 : 2 }, (_, i) => change(i + 7, -(30 - i * 5))),
            history: Array.from({ length: full ? 30 : 12 }, (_, i) => ({ label: label(i + 3), points: i % 3 === 2 ? -(3 + i % 5) : 2 + i % 6, day: 2 + (count - 1 - i) * 2 }))
        };
    }

    const identityOf = row => row.referenceFormID && row.referenceFormID !== '00000000' ? row.referenceFormID : row.baseFormID;
    const findRow = identity => state.romances.find(row => identityOf(row) === identity);
    // A sealed page as RomantasyUI keeps it: a favorite (2) is also a like, a pet peeve (-2) also a dislike.
    function preferencesOf(page) {
        const likes = [], dislikes = [], favorites = [], petPeeves = [];
        for (const entry of Array.isArray(page) ? page : []) {
            const option = state.preferenceOptions.find(o => o.editorID === entry[0]);
            const label = option ? option.label : String(entry[0]);
            if (entry[1] > 0) likes.push(label);
            else if (entry[1] < 0) dislikes.push(label);
            if (entry[1] === 2) favorites.push(label);
            else if (entry[1] === -2) petPeeves.push(label);
        }
        return { likes, dislikes, favorites, petPeeves };
    }

    function levelChange(kind) {
        const rows = state.romances;
        if (kind === 'loss') {
            return { followerName: rows[1] ? rows[1].name : 'Serana', changeDirection: 'loss', previousLevelName: 'Friend', previousLevelIndex: 3,
                levelName: 'Acquaintance', levelIndex: 2, nextLevelName: 'Friend', points: 985, pointsDelta: -30, unlocks: [] };
        }
        return { followerName: rows[0] ? rows[0].name : 'Aelira', changeDirection: 'gain', previousLevelName: 'Confidant', previousLevelIndex: 4,
            levelName: 'Lover', levelIndex: 5, nextLevelName: 'Spouse', points: 2010, pointsDelta: 25,
            unlocks: ['A key to her room at the Bannered Mare', 'Her lute, and the songs she wrote for you'] };
    }

    // One branch per bond kind, with RomantasyUI's outcome messages.
    const bond = {
        enroll(p) {
            console.log('mock: enroll ' + p.identity + ' with ' + (Array.isArray(p.preferences) ? p.preferences.length : 0) + ' preferences', p.preferences);
            const candidate = state.enrollmentCandidates.find(c => identityOf(c) === p.identity);
            if (fail || !candidate) return 'That companion could not be added.';
            const prefs = preferencesOf(p.preferences);
            state.romances.push({
                name: candidate.name, editorID: '', baseFormID: candidate.baseFormID, referenceFormID: candidate.referenceFormID,
                preferencesManual: true, profileOrigin: 'player', personalityEditable: true, removable: true,
                sourcePlugin: candidate.sourcePlugin || '', profileFile: '', points: 0, levelName: 'Stranger', nextGoal: 'Acquaintance at 500',
                isFollowing: true, role: candidate.role || '', likes: prefs.likes, dislikes: prefs.dislikes,
                favorites: prefs.favorites, petPeeves: prefs.petPeeves, unlocks: [], recent: []
            });
            state.enrollmentCandidates = state.enrollmentCandidates.filter(c => c !== candidate);
            return 'A new bond was entered in the ledger.';
        },
        replace(p) {
            console.log('mock: replace ' + p.identity + ' with ' + (Array.isArray(p.preferences) ? p.preferences.length : 0) + ' preferences', p.preferences);
            const row = findRow(p.identity);
            if (fail || !row) return 'The personality could not be saved. Your previous preferences are unchanged.';
            Object.assign(row, preferencesOf(p.preferences));
            return 'Companion personality sealed.';
        },
        reset(p) {
            const row = findRow(p.identity);
            if (fail || !row) return 'That bond cannot be reset here.';
            row.points = 0; row.levelName = 'Stranger'; row.recent = [];
            return 'Player-created bond reset.';
        },
        remove(p) {
            const row = findRow(p.identity);
            if (fail || !row) return 'The bond could not be removed. Your existing bond is unchanged.';
            state.romances = state.romances.filter(r => r !== row);
            return 'Player-created bond removed.';
        }
    };

    // As RomantasyUI::PublishMode: the mode carries the three comfort settings, so a popup follows them with no state.
    const modeMessage = mode => ({ mode, ledgerSize: state.ledgerSize, highContrast: state.highContrast, reduceMotion: state.reduceMotion });

    const handlers = {
        romantasyReady() {
            // Mode before UI mode: the page asks again when UI mode comes on while it is still hidden,
            // and this harness answers synchronously, so the other order would never end.
            publish('romantasyMode', modeMessage(param('mode') === 'popup' ? 'popup' : 'ledger'));
            publish('__uimode', '1');
            sendState('Romance records synchronized.');
            const popup = param('popup');
            if (popup === 'gain' || popup === 'loss') mock.popup(popup);
        },
        // Ctrl + R over a popup alone: the ledger opens around it, as RomantasyUI::Toggle does.
        romantasyOpen() {
            publish('romantasyMode', modeMessage('ledger'));
            sendState('Romance records synchronized.');
        },
        romantasyClose() { publish('__uimode', '0'); console.log('mock: ledger closed (reload to reopen)'); },
        romantasyRefresh() { sendState('Ledger refreshed.'); },
        romantasySettings(p) {
            for (const key of ['openWithFavorites', 'showGainModals', 'showLossModals', 'showAwayFollowers', 'whispers', 'detailedLog', 'highContrast', 'reduceMotion']) {
                if (typeof p[key] === 'boolean') state[key] = p[key];
            }
            if (['slow', 'standard', 'fast'].includes(p.pace)) state.pace = p.pace;
            if (['calm', 'immediate'].includes(p.tierMoment)) state.tierMoment = p.tierMoment;
            if (['topLeft', 'topRight', 'bottomLeft', 'bottomRight'].includes(p.whisperCorner)) state.whisperCorner = p.whisperCorner;
            if (['closest', 'name', 'recent'].includes(p.rosterOrder)) state.rosterOrder = p.rosterOrder;
            if (['standard', 'large', 'largest'].includes(p.ledgerSize)) state.ledgerSize = p.ledgerSize;
            if (typeof p.skyrimNetBridge === 'boolean') state.skyrimNet.bridge = p.skyrimNetBridge;
            if (typeof p.skyrimNetConversation === 'boolean') state.skyrimNet.conversation = p.skyrimNetConversation;
            if (typeof p.ostimMoments === 'boolean') state.ostim.moments = p.ostimMoments;
            if (typeof p.ostimHold === 'boolean') state.ostim.hold = p.ostimHold;
            sendState('Preferences noted.');
        },
        romantasyBond(p) {
            const kind = String(p.kind || '').toLowerCase();
            sendState(bond[kind] ? bond[kind](p) : 'That request was refused.');
        },
        romantasyDevUnlock(p) {
            if (p.password === 'mock') state.developerToolsUnlocked = true;
            sendState('');
        },
        romantasyDevLock() { state.developerToolsUnlocked = false; sendState(''); },
        romantasyDebug(p) {
            if (!state.developerToolsUnlocked) { console.warn('mock: debug request blocked; developer tools are locked'); return; }
            // { points } moves every following bond; { stat, delta } stands in for a deed: a following
            // bond that likes the stat gains 5 points per count, one that dislikes it loses 5; a favorite or pet peeve doubles it.
            const delta = row => {
                if (typeof p.points === 'number') return p.points;
                const per = ((row.likes || []).includes(p.stat) ? 5 : (row.dislikes || []).includes(p.stat) ? -5 : 0)
                    * ((row.favorites || []).includes(p.stat) || (row.petPeeves || []).includes(p.stat) ? 2 : 1);
                return per * (Number(p.delta) || 0);
            };
            if (typeof p.points !== 'number' && typeof p.stat !== 'string') return;
            for (const row of state.romances) {
                if (!row.isFollowing) continue;
                row.points = Math.max(0, row.points + delta(row));
                row.levelName = levelName(row.points);
            }
            sendState('');
        },
        romantasyPopupShown() {},
        romantasyPopupDismissed() {},
        // One bond's journal, as RomantasyUI answers: found false for an identity that is not a bond (and with &fail).
        romantasyJournalOpen(p) {
            const row = findRow(p.identity);
            publish('romantasyJournal', fail || !row ? { identity: String(p.identity), found: false } : journalOf(row, p.identity));
        },
        // The helper's ready (with &pad): bind it, then put the pad in use.
        __gamepad(p) {
            if (p.op !== 'ready') return;
            pad.page = p.page;
            if (window.RM && window.RM.pad) window.RM.pad.attach();
            pad.send({});
        }
    };
    mock.handlers = handlers;

    // ── The pad, with &pad: packets as Magelight's host sends them (Tailor's dev/mock.js) ──────────
    const CONTROL_OF = { up: 'dpadUp', down: 'dpadDown', left: 'dpadLeft', right: 'dpadRight', accept: 'south', cancel: 'east', previousTab: 'leftShoulder', nextTab: 'rightShoulder', secondary: 'west', tertiary: 'north' };
    const pad = { page: '', seq: 0 };
    pad.packet = over => Object.assign({ version: 1, page: pad.page, generation: 1, sequence: ++pad.seq, active: true, reset: false, enabled: true, connected: true,
        style: 'navigation', mode: 'navigation', device: 'gamepad', glyphFamily: 'xbox', bindings: Object.assign({}, CONTROL_OF), dt: 0.016, events: [] }, over);
    pad.send = over => publish('__gamepad', pad.packet(over));
    pad.event = (action, phase) => ({ control: CONTROL_OF[action] || 'none', action, phase, value: phase === 'release' ? 0 : 1, x: 0, y: 0 });
    pad.press = action => { pad.send({ events: [pad.event(action, 'press')] }); pad.send({ events: [pad.event(action, 'release')] }); };
    pad.keyboard = () => pad.send({ device: 'keyboardMouse' });
    mock.pad = pad;

    function handle(name, payload) {
        mock.log.push({ name, payload });
        // Never the typed password, not even in the harness.
        const shown = payload && typeof payload === 'object' && 'password' in payload ? Object.assign({}, payload, { password: '(hidden)' }) : payload;
        console.log('mock: ' + name, shown);
        const fn = handlers[name];
        if (!fn) { console.warn('mock: unknown action ' + name); return; }
        fn(payload && typeof payload === 'object' ? payload : {});
    }

    const RM = window.RM || {};
    if (RM.bridge && !RM.bridge.host) RM.bridge.attach(window.magelight);
    if (RM.app) RM.app.connect();   // the page booted before the stand-in existed
    console.log('Romantasy mock ready' + (q.length > 5 ? ': ' + q.slice(1) : ''));
})();
