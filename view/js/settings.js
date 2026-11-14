// The settings pane: nine switches and four choice rows (two more switches each when SkyrimNet or OStim was found), once unlocked
// the developer card, and the Health section
// (when the state carries a report: first while it is warn or error, else at the foot). paneHtml is
// pure and runs under Node for the tests; render() below the export line paints #pane
// while view.pane is 'settings'.
//
// Page events heard: open-settings. Actions (all prefixed settings.):
//   settings.back             settings.toggle (data-key)       settings.choose (data-key, data-value)
//   settings.dev-bond-up      settings.dev-bond-down     settings.dev-side-quest
//   settings.dev-murder       settings.dev-refresh
(function () {
    'use strict';
    const node = typeof module === 'object' && module.exports;
    const format = node ? require('./format.js') : window.RM.format;
    const model = node ? require('./state.js') : window.RM.state;
    const S = node ? require('./strings.js') : window.RM.strings;
    const esc = format.esc;

    const icon = key => '<img class="art" src="icons/' + key + '.png" alt="" draggable="false">';

    // [state key, jewel icon, title, detail]
    const SWITCHES = [
        ['openWithFavorites', 'star', S.t('settings.favorites'), S.t('settings.favorites.detail')],
        ['showGainModals', 'heart', S.t('settings.gain'), S.t('settings.gain.detail')],
        ['showLossModals', 'heartbroken', S.t('settings.loss'), S.t('settings.loss.detail')],
        ['showAwayFollowers', 'eye', S.t('settings.away'), S.t('settings.away.detail')],
        ['whispers', 'heart', S.t('settings.whispers'), S.t('settings.whispers.detail')],
        ['highContrast', 'eye', S.t('settings.contrast'), S.t('settings.contrast.detail')],
        ['reduceMotion', 'neutral', S.t('settings.motion'), S.t('settings.motion.detail')],
        ['detailedLog', 'search', S.t('settings.log'), S.t('settings.log.detail')],
        ['developerToolsUnlocked', 'lock', S.t('settings.dev'), S.t('settings.dev.detail')]
    ];
    // [settings key, jewel icon, title, detail]: drawn only when SkyrimNet was found; their values live in state.skyrimNet.
    const SKYRIMNET = [
        ['skyrimNetBridge', 'rays', S.t('settings.skyrimnet.bridge'), S.t('settings.skyrimnet.bridge.detail')],
        ['skyrimNetConversation', 'confidant', S.t('settings.skyrimnet.conversation'), S.t('settings.skyrimnet.conversation.detail')]
    ];
    // [settings key, jewel icon, title, detail]: drawn only when OStim was found; their values live in state.ostim.
    const OSTIM = [
        ['ostimMoments', 'lover', S.t('settings.ostim.moments'), S.t('settings.ostim.moments.detail')],
        ['ostimHold', 'neutral', S.t('settings.ostim.hold'), S.t('settings.ostim.hold.detail')]
    ];
    // [state key, jewel icon, title, [[value, label]], detail]; an empty detail draws no detail line.
    const CHOICES = [
        ['tierMoment', 'neutral', S.t('settings.moment'), [['calm', S.t('settings.moment.calm')], ['immediate', S.t('settings.moment.immediate')]],
            S.t('settings.moment.detail')],
        ['pace', 'likes', S.t('settings.pace'), [['slow', S.t('settings.pace.slow')], ['standard', S.t('settings.pace.standard')], ['fast', S.t('settings.pace.fast')]],
            S.t('settings.pace.detail')],
        ['whisperCorner', 'eye', S.t('settings.corner'), [['topLeft', S.t('settings.corner.topleft')], ['topRight', S.t('settings.corner.topright')],
            ['bottomLeft', S.t('settings.corner.bottomleft')], ['bottomRight', S.t('settings.corner.bottomright')]], ''],
        ['ledgerSize', 'search', S.t('settings.size'), [['standard', S.t('settings.size.standard')], ['large', S.t('settings.size.large')], ['largest', S.t('settings.size.largest')]],
            S.t('settings.size.detail')]
    ];
    // Every row, top to bottom, by state key.
    const ORDER = ['openWithFavorites', 'showGainModals', 'showLossModals', 'showAwayFollowers', 'tierMoment', 'pace',
        'whispers', 'whisperCorner', 'skyrimNetBridge', 'skyrimNetConversation', 'ostimMoments', 'ostimHold', 'ledgerSize',
        'highContrast', 'reduceMotion', 'detailedLog', 'developerToolsUnlocked'];
    const skyrimNet = state => (state.skyrimNet && typeof state.skyrimNet === 'object') ? state.skyrimNet : {};
    const ostim = state => (state.ostim && typeof state.ostim === 'object') ? state.ostim : {};
    // A row left out entirely in this state: no gap, no focus stop.
    const SHOWN_WHEN = { skyrimNetBridge: state => skyrimNet(state).found === true, skyrimNetConversation: state => skyrimNet(state).found === true,
        ostimMoments: state => ostim(state).found === true, ostimHold: state => ostim(state).found === true };
    const isShown = (key, state) => !SHOWN_WHEN[key] || SHOWN_WHEN[key](state);
    // A row that cannot be used in this state: drawn dimmed, a disabled switch or a choice of disabled buttons, which
    // the keys, Tab and the pad pass over and a click cannot press.
    const OFF_WHEN = { whisperCorner: state => state.whispers !== true, skyrimNetConversation: state => skyrimNet(state).bridge !== true };
    const isOff = (key, state) => !!OFF_WHEN[key] && OFF_WHEN[key](state);
    // A switch's value: its own state key, or for the SkyrimNet and OStim switches the field of state.skyrimNet or state.ostim.
    const VALUE_OF = { skyrimNetBridge: state => skyrimNet(state).bridge, skyrimNetConversation: state => skyrimNet(state).conversation,
        ostimMoments: state => ostim(state).moments, ostimHold: state => ostim(state).hold };
    const isOn = (key, state) => (VALUE_OF[key] ? VALUE_OF[key](state) : state[key]) === true;
    const switchOf = key => SWITCHES.find(s => s[0] === key) || SKYRIMNET.find(s => s[0] === key) || OSTIM.find(s => s[0] === key);
    // The rows drawn in this state, top to bottom.
    const shownRows = state => ORDER.filter(key => isShown(key, state));
    // [action, label, what it sends]
    const DEV = [
        ['settings.dev-bond-up', S.t('dev.bond.up'), ['romantasyDebug', { points: 500 }]],
        ['settings.dev-bond-down', S.t('dev.bond.down'), ['romantasyDebug', { points: -500 }]],
        ['settings.dev-side-quest', S.t('dev.sidequest'), ['romantasyDebug', { stat: 'Side Quests Completed', delta: 1 }]],
        ['settings.dev-murder', S.t('dev.murder'), ['romantasyDebug', { stat: 'Murders', delta: 1 }]],
        ['settings.dev-refresh', S.t('dev.refresh'), ['romantasyRefresh']]
    ];
    const NOTE = S.t('dev.note');

    function switchHtml([key, jewel, title, detail], on, i, off) {
        return '<button class="setrow glass" role="switch" aria-checked="' + (on ? 'true' : 'false') + '" data-action="settings.toggle" data-key="' + key + '"'
            + (off ? ' disabled aria-disabled="true"' : '') + ' style="--i:' + i + '">'
            + '<span class="jewel">' + icon(jewel) + '</span>'
            + '<span class="text"><b class="title">' + esc(title) + '</b><span class="detail">' + esc(detail) + '</span></span>'
            + '<span class="switch"><span class="knob"></span></span></button>';
    }

    // A choice row: the switch row's glass shape, with one button per option in place of the switch; the current
    // one is checked (filled). The row is one focus stop: pad.js walks its options with Left and Right. A row that is
    // off says so on the group and disables its options.
    function choiceHtml([key, jewel, title, options, detail], value, i, off) {
        return '<div class="setrow glass choice" role="radiogroup" aria-label="' + esc(title) + '"' + (off ? ' aria-disabled="true"' : '') + ' style="--i:' + i + '">'
            + '<span class="jewel">' + icon(jewel) + '</span>'
            + '<span class="text"><b class="title">' + esc(title) + '</b>' + (detail ? '<span class="detail">' + esc(detail) + '</span>' : '') + '</span>'
            + '<span class="choices">' + options.map(([v, label]) => '<button class="opt" role="radio" aria-checked="' + (v === value ? 'true' : 'false')
                + '" data-action="settings.choose" data-key="' + key + '" data-value="' + v + '"' + (off ? ' disabled' : '') + '>' + esc(label) + '</button>').join('')
            + '</span></div>';
    }

    // Every row in ORDER that this state shows; --i counts them, so the entrance runs top to bottom.
    function rowsHtml(state) {
        return shownRows(state).map((key, i) => {
            const sw = switchOf(key);
            if (sw) return switchHtml(sw, isOn(key, state), i, isOff(key, state));
            return choiceHtml(CHOICES.find(c => c[0] === key), state[key], i, isOff(key, state));
        }).join('');
    }

    function devHtml() {
        return '<section class="devcard glass"><h3 class="eyebrow">' + esc(S.t('dev.title')) + '</h3><p class="note">' + esc(NOTE) + '</p><div class="devbtns">'
            + DEV.map(([action, label]) => '<button class="btn" data-action="' + action + '">' + esc(label) + '</button>').join('')
            + '</div></section>';
    }

    // The Health section: a status list, not controls. Warn and error carry a word beside the mark;
    // a note has a quiet mark only. Empty when the state has no health report.
    const WORD = model.HEALTH_WORDS;
    function itemHtml(item) {
        return '<li class="hitem" data-level="' + item.level + '"><span class="hmark"></span><span class="htext">'
            + '<b class="htitle">' + (WORD[item.level] ? '<span class="hword">' + esc(WORD[item.level]) + '</span>' : '') + esc(item.title) + '</b>'
            + '<span class="hdetail">' + item.detail.split('\n').map(line => '<span class="hline">' + esc(line) + '</span>').join('') + '</span>'
            + '</span></li>';
    }
    function healthHtml(state) {
        const h = state.health;
        if (!h) return '';
        const calm = h.status === 'ok';   // the one source for the mark, the placement and this line
        const meta = [h.version ? S.t('health.meta.plugin.version', { version: h.version }) : S.t('health.meta.plugin')]
            .concat(h.host ? [S.t('health.meta.host', { version: h.host })] : [], [S.t(h.bonds === 1 ? 'health.meta.bonds.one' : 'health.meta.bonds.many', { count: h.bonds })]).join(' · ');
        return '<section class="healthcard glass"><h3 class="eyebrow">' + esc(S.t('health.title')) + '</h3>'
            + (calm ? '<p class="hok">' + esc(S.t('health.ok')) + '</p><p class="hmeta">' + esc(meta) + '</p>' : '')
            + (h.items.length ? '<ul class="hitems">' + h.items.map(itemHtml).join('') + '</ul>' : '')
            + '</section>';
    }

    // While the status is warn or error, Health comes first in the list so a player sent here sees it;
    // otherwise it sits at the foot.
    function paneHtml(state) {
        const first = model.healthAlert(state);
        return '<header class="sethead glass">'
            + '<div class="titles"><span class="eyebrow">' + esc(S.t('settings.eyebrow')) + '</span><h2 class="foil">' + esc(S.t('settings.title')) + '</h2></div>'
            + '<button class="btn" data-action="settings.back"><span class="glyph back"></span>' + esc(S.t('common.back')) + '</button></header>'
            + '<div class="setlist scroller">' + (first ? healthHtml(state) : '') + rowsHtml(state)
            + (state.developerToolsUnlocked ? devHtml() : '') + (first ? '' : healthHtml(state)) + '</div>';
    }

    const api = { paneHtml, healthHtml, SWITCHES, SKYRIMNET, OSTIM, CHOICES, DEV };
    if (node) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.settings = api;

    /* ---------- DOM ---------- */
    const RM = window.RM;
    let painted = false, healthPainted = '', rowsPainted = '';

    // The pane is painted once when it opens. After that a state only moves the switches
    // (so the knob slides rather than jumps), folds the developer card in or out above the
    // Health section, and repaints Health when its report changed.
    // What the Health section shows and where: a change of either repaints it.
    const healthKey = state => (model.healthAlert(state) ? 'first:' : 'foot:') + healthHtml(state);

    function render(state, view) {
        const pane = document.getElementById('pane');
        if (!pane) return;
        if (view.pane !== 'settings') { painted = false; return; }
        // A row that comes or goes (SkyrimNet or OStim found or not) repaints the pane: rows are never moved in place.
        if (!painted || !pane.querySelector('.setlist') || rowsPainted !== shownRows(state).join()) {
            pane.className = 'settings enter';
            pane.innerHTML = paneHtml(state);
            painted = true;
            healthPainted = healthKey(state);
            rowsPainted = shownRows(state).join();
            setTimeout(() => pane.classList.remove('enter'), 900);
            return;
        }
        for (const el of pane.querySelectorAll('[role="switch"]')) {
            const key = el.getAttribute('data-key');
            el.setAttribute('aria-checked', isOn(key, state) ? 'true' : 'false');
            el.disabled = isOff(key, state);
            if (el.disabled) el.setAttribute('aria-disabled', 'true');
            else el.removeAttribute('aria-disabled');
        }
        for (const el of pane.querySelectorAll('[role="radio"]')) {
            const key = el.getAttribute('data-key');
            el.setAttribute('aria-checked', state[key] === el.getAttribute('data-value') ? 'true' : 'false');
            el.disabled = isOff(key, state);
        }
        for (const group of pane.querySelectorAll('[role="radiogroup"]')) {
            const option = group.querySelector('[role="radio"]');
            if (!option) continue;
            if (isOff(option.getAttribute('data-key'), state)) group.setAttribute('aria-disabled', 'true');
            else group.removeAttribute('aria-disabled');
        }
        const list = pane.querySelector('.setlist'), card = pane.querySelector('.devcard');
        if (state.developerToolsUnlocked && !card) {
            const health = list.querySelector('.healthcard');
            if (health && health === list.lastElementChild) health.insertAdjacentHTML('beforebegin', devHtml());
            else list.insertAdjacentHTML('beforeend', devHtml());
            const added = pane.querySelector('.devcard');
            if (added) added.classList.add('unfold');
        } else if (!state.developerToolsUnlocked && card) {
            card.remove();
        }
        const key = healthKey(state);
        if (key !== healthPainted) {
            const old = list.querySelector('.healthcard');
            if (old) old.remove();
            list.insertAdjacentHTML(model.healthAlert(state) ? 'afterbegin' : 'beforeend', healthHtml(state));
            healthPainted = key;
        }
    }

    const app = () => RM.app;
    const actions = {
        'settings.back': () => { app().view.pane = 'dossier'; app().render(); },
        'settings.toggle': el => {
            const key = el.getAttribute('data-key');
            const on = isOn(key, app().state);
            if (key === 'developerToolsUnlocked') {
                if (on) RM.bridge.send('romantasyDevLock');
                else { app().view.modal = { kind: 'password' }; app().render(); }
                return;
            }
            if (!switchOf(key) || !isShown(key, app().state) || isOff(key, app().state)) return;
            RM.bridge.send('romantasySettings', { [key]: !on });
        },
        // An option of a choice row sends its one key; the current option sends nothing. The next state checks it.
        'settings.choose': el => {
            const key = el.getAttribute('data-key'), value = el.getAttribute('data-value');
            const choice = CHOICES.find(c => c[0] === key);
            if (!choice || !choice[3].some(o => o[0] === value) || app().state[key] === value || isOff(key, app().state)) return;
            RM.bridge.send('romantasySettings', { [key]: value });
        }
    };
    for (const [action, , message] of DEV) actions[action] = () => RM.bridge.send(message[0], message[1]);

    function init(appApi) {
        appApi.on('open-settings', () => { appApi.view.pane = 'settings'; appApi.render(); });
    }

    (RM.parts = RM.parts || []).push({ name: 'settings', render, actions, init });
})();
