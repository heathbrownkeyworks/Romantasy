// The ledger: the roster of bonds on the left, the selected bond's dossier (or the
// empty state) on the right. The *Html functions are pure and run under Node for
// the tests; render() below the export line paints #roster and #pane.
//
// Controls are <button data-action>; app.js owns the one delegated click listener
// and calls this part's actions[name](button). The dossier's Journal button runs journal.js's journal.open. Page events emitted:
//   select (identity)      open-settings          refresh          close
//   open-enroll ({ mode: 'create' } | { mode: 'edit', identity })
//   confirm ({ kind: 'reset' | 'remove', identity })
// Roster actions: roster.clear (empties the find text), roster.order (data-value: sends that order).
//
// The roster: a head (find field, order choice) over the rows in their groups, as roster.js arranges them for
// view.find. The head is painted once and never repainted, so typing keeps its field; a state only moves the
// order choice's check and the clear button's state.
(function () {
    'use strict';
    const node = typeof module === 'object' && module.exports;
    const tiers = node ? require('./tiers.js') : window.RM.tiers;
    const format = node ? require('./format.js') : window.RM.format;
    const model = node ? require('./state.js') : window.RM.state;
    const rules = node ? require('./roster.js') : window.RM.roster;
    const S = node ? require('./strings.js') : window.RM.strings;
    const esc = format.esc;
    // A page word, escaped where it is drawn (a value in its places is text, never markup).
    const tx = (key, values) => esc(S.t(key, values));

    const icon = (key, cls) => '<img class="' + (cls || 'art') + '" src="icons/' + key + '.png" alt="" draggable="false">';
    const pct = f => (Math.round(Math.min(1, Math.max(0, f)) * 1000) / 10) + '%';
    const role = row => esc(row.role || S.t('common.companion'));

    // Four-pointed sparks around the hero heart: [x, y, scale, duration (s), delay (s)].
    const SPARKS = [['8%', '20%', 1.2, 9, -1], ['84%', '14%', .8, 7, -3], ['90%', '66%', 1.4, 11, -5], ['12%', '76%', .7, 8, -2], ['70%', '88%', .9, 10, -6]]
        .map(([x, y, s, d, delay]) => '<svg class="spark" viewBox="0 0 24 24" style="--x:' + x + ';--y:' + y + ';--s:' + s + ';--d:' + d + 's;--delay:' + delay
            + 's"><path d="M12 0c1 8 4 11 12 12-8 1-11 4-12 12-1-8-4-11-12-12 8-1 11-4 12-12z"/></svg>').join('');

    // How the personality's origin is worded: four cases, in order.
    function origin(row) {
        const playerCreated = row.profileOrigin === 'player' && row.personalityEditable;
        if (!playerCreated && row.profileFile) {
            return [S.t('persona.configured'), S.t('persona.configured.line', { file: row.profileFile })];
        }
        if (playerCreated) {
            return [S.t('persona.player'), S.t('persona.player.line')];
        }
        if (row.profileOrigin === 'author') {
            return [S.t('persona.author'), row.sourcePlugin ? S.t('persona.author.line.plugin', { plugin: row.sourcePlugin }) : S.t('persona.author.line')];
        }
        return [S.t('persona.managed'), S.t('persona.managed.line')];
    }

    function rowHtml(row, i, selected) {
        const identity = model.rowIdentity(row);
        const t = tiers.tier(row.points);
        const cls = 'row' + (identity === selected ? ' selected' : '') + (row.isFollowing ? '' : ' away');
        return '<button class="' + cls + '" data-action="select" data-identity="' + esc(identity) + '" data-ml-id="row-' + esc(identity) + '" style="--i:' + i + '">'
            + icon(t.key) + '<span class="dot' + (row.isFollowing ? ' on' : '') + '"></span>'
            + '<span class="who"><small class="role">' + (row.isFollowing ? '' : '<span class="away-tag">' + tx('roster.row.away') + '</span>') + role(row) + '</small>'
            + '<b class="name">' + esc(row.name) + '</b>'
            + '<span class="bar"><span class="fill" style="width:' + pct(tiers.tierProgress(row.points)) + '"></span></span></span>'
            + '<span class="rank"><em class="tiername">' + esc(tiers.shown(row.levelName)) + '</em><span class="pts">' + format.pointsWithThreshold(row.points) + '</span></span>'
            + '</button>';
    }

    // [value, label key] of the order choice, in its order.
    const ORDERS = [['closest', 'roster.order.closest'], ['name', 'roster.order.name'], ['recent', 'roster.order.recent']];
    const findOf = view => view && typeof view.find === 'string' ? view.find : '';
    // Find text matched no one: there is text, there are rows to show without it, and none of them matches. The roster
    // and the pane's empty state both ask this, so they never disagree.
    const nobodyFound = (state, view) => findOf(view).trim() !== '' && model.visibleRows(state).length > 0 && !rules.shownRows(state, view).length;

    // The counts are two unbreakable parts: one line while they fit beside the heading, two (under its height) when not.
    function companyHtml(state) {
        const counts = tiers.countBonds(state.romances, state.showAwayFollowers);
        return '<em class="foil" title="' + tx('roster.company') + '">' + tx('roster.company') + '</em><span class="counts"><span>'
            + tx(counts.bonds === 1 ? 'roster.count.one' : 'roster.count.many', { count: counts.bonds }) + '</span> <span>'
            + tx(counts.following === 1 ? 'roster.following.one' : 'roster.following.many', { count: counts.following }) + '</span></span>';
    }

    // The find field never carries the find text in its markup: render() sets the field's value.
    function headHtml(state, view) {
        return '<div class="rhead"><div class="find">' + icon('search', 'art seek')
            + '<input class="findbox" type="text" autocomplete="off" spellcheck="false" placeholder="' + tx('roster.find') + '" aria-label="' + tx('roster.find') + '" data-ml-id="roster-find">'
            + '<button class="clear" title="' + tx('roster.find.clear') + '" aria-label="' + tx('roster.find.clear') + '" data-ml-id="roster-clear" data-action="roster.clear"' + (findOf(view) === '' ? ' disabled' : '') + '>'
            + '<span class="glyph close"></span></button></div>'
            + '<div class="order" role="radiogroup" aria-label="' + tx('roster.order.label') + '">' + ORDERS.map(([v, label]) => '<button class="opt" role="radio" aria-checked="'
                + (v === state.rosterOrder ? 'true' : 'false') + '" data-action="roster.order" data-value="' + v + '" title="' + tx(label) + '">' + tx(label) + '</button>').join('')
            + '</div></div>';
    }

    // The groups' headings and rows, or the line that says the find text matched no one.
    function groupsHtml(state, view) {
        const selected = view ? view.selected : null;
        const groups = rules.arrangeFor(state, view).groups;
        if (!groups.length) return nobodyFound(state, view) ? '<p class="none">' + tx('roster.nobody') + '</p>' : '';
        let i = 0;
        return groups.map(g => '<h3 class="group" data-group="' + g.key + '"><span class="gname">' + esc(g.title) + '</span><span class="gcount">' + g.rows.length + '</span></h3>'
            + g.rows.map(row => rowHtml(row, i++, selected)).join('')).join('');
    }

    function railHtml(state) {
        return '<button class="btn primary" data-action="new-bond">' + icon('newbond') + tx('common.newbond') + '</button>'
            + '<button class="btn round" data-action="settings" title="' + tx('roster.settings') + '">' + icon('settings')
            + (model.healthAlert(state) ? '<span class="mark" data-level="' + state.health.status + '" role="img" aria-label="'
                + tx('roster.health', { status: model.HEALTH_WORDS[state.health.status] }) + '"></span>' : '') + '</button>'
            + '<button class="btn round" data-action="refresh" title="' + tx('roster.refresh') + '">' + icon('refresh') + '</button>'
            + '<button class="btn round" data-action="close" title="' + tx('roster.close') + '"><span class="glyph close"></span></button>';
    }

    function rosterHtml(state, view) {
        return '<div class="brand">' + icon('heart') + '<h1>' + tx('brand.title') + '</h1></div>'
            + '<div class="company">' + companyHtml(state) + '</div>'
            + headHtml(state, view)
            + '<div class="rows scroller">' + groupsHtml(state, view) + '</div>'
            + '<div class="rail">' + railHtml(state) + '</div>';
    }

    // One side of "What they value": the strong ones (favorites, pet peeves) first, each with its tag,
    // then the rest. Every row shows what one event is worth for this follower: points x weight.
    function listHtml(items, strong, sign, options, none) {
        const list = Array.isArray(items) ? items : [], marked = (Array.isArray(strong) ? strong : []).filter(x => list.includes(x));
        if (!list.length) return '<li class="none">' + none + '</li>';
        const ordered = marked.concat(list.filter(x => !marked.includes(x)));
        const like = sign > 0;
        return ordered.map(item => {
            const isStrong = marked.includes(item);
            const option = (Array.isArray(options) ? options : []).find(o => o && o.label === item);
            // A deed with an editor ID shows in the table's words when strings.js has a line for it, else as the plugin wrote it.
            const shownItem = option && typeof option.editorID === 'string' ? S.or('deed.' + option.editorID, item) : item;
            const worth = option && typeof option.points === 'number' && isFinite(option.points)
                ? '<b class="worth">' + format.signed(option.points * sign * (isStrong ? 2 : 1)) + '</b>' : '';
            const key = isStrong ? (like ? 'star' : 'heartbroken') : (like ? 'likes' : 'dislikes');
            return '<li class="item' + (isStrong ? (like ? ' favorite' : ' peeve') : '') + '">' + icon(key)
                + '<span class="text"><span class="label">' + esc(shownItem) + '</span>'
                + (isStrong ? '<small class="kind">' + tx(like ? 'common.favorite' : 'common.peeve') + '</small>' : '') + '</span>' + worth + '</li>';
        }).join('');
    }

    // "Confidant unlocks: A key to her house · Her mother's ring": the next tier's lines, or nothing. Every line
    // shows: they wrap to two lines across from the heading, and scroll inside the block past that.
    function unlocksHtml(row, index) {
        const next = tiers.TIERS[index + 1];
        if (!next) return '';
        const entry = (row.unlocks || []).find(u => u && u.level === index + 2);
        if (!entry || !entry.items.length) return '';
        return '<div class="unlocks scroller" tabindex="0" data-ml-id="body-unlocks"><p class="lines"><em class="next">' + tx('miles.unlocks', { tier: next.name }) + '</em> '
            + entry.items.map(esc).join(' · ') + '</p></div>';
    }

    // The mood chip beside Following: [word key, title key]. A mood outside these shows no chip.
    const MOOD = {
        warm: ['mood.warm', 'mood.warm.title'],
        settled: ['mood.settled', 'mood.settled.title'],
        strained: ['mood.strained', 'mood.strained.title']
    };
    function moodChip(row) {
        const m = Object.prototype.hasOwnProperty.call(MOOD, row.mood) ? MOOD[row.mood] : null;
        return m ? '<span class="chip mood ' + row.mood + '" title="' + tx(m[1]) + '">' + tx(m[0]) + '</span>' : '';
    }

    // "Friend needs 3 days together (1.5 so far)": Slow burn's gate ahead, under the meter, or nothing.
    // Days so far are cut down to one decimal place, never rounded up, so the line cannot show the
    // needed number while the gate is still shut.
    function gateHtml(gate) {
        if (!gate) return '';
        const so = (Math.floor(gate.daysTogether * 10 + 1e-6) / 10).toFixed(1);
        return '<p class="gate">' + tx('dossier.gate', { tier: tiers.shown(gate.name), needed: Math.round(gate.daysNeeded), sofar: so }) + '</p>';
    }

    function nameClass(name) {
        const n = Array.from(name).length;
        return n > 18 ? 'name longer' : n > 12 ? 'name long' : 'name';
    }

    // options: state.preferenceOptions, for what one event is worth (rows show no value without them).
    function dossierHtml(row, options) {
        const identity = esc(model.rowIdentity(row));
        const index = tiers.tierIndex(row.points);
        const next = tiers.nextThreshold(row.points);
        const top = next < 0;
        const thread = (index + (top ? 0 : tiers.tierProgress(row.points))) / (tiers.TIERS.length - 1);

        // The bond tools, in the head of the Milestones card. Each carries its label as a title too: a label too long for
        // the row ends in an ellipsis there.
        const tool = (cls, action, key) => '<button class="' + cls + '" data-action="' + action + '" data-target="' + identity + '" title="' + tx(key) + '">' + tx(key) + '</button>';
        let tools = '';
        if (row.personalityEditable) tools += tool('tool', 'edit', 'dossier.edit');
        if (row.removable) tools += tool('tool', 'reset', 'dossier.reset') + tool('tool danger', 'remove', 'dossier.remove');

        // The eyeline: the role (it gives way), then the chips (they never do).
        const hero = '<section class="hero glass">'
            + '<div class="heartwrap"><span class="glow"></span><span class="halo"></span><span class="halo b"></span><span class="halo c"></span>'
            + icon('heart', 'art heart') + SPARKS + '</div>'
            + '<div class="id">'
            + '<div class="eyeline"><span class="eyebrow">' + role(row) + '</span><span class="chip' + (row.isFollowing ? ' on' : '') + '">'
            + tx(row.isFollowing ? 'dossier.following' : 'dossier.away') + '</span>' + moodChip(row)
            + '</div>'
            + '<h2 class="' + nameClass(row.name) + '" title="' + esc(row.name) + '">' + esc(row.name) + '</h2>'
            + '<span class="tier foil">' + esc(tiers.shown(row.levelName)) + '</span>'
            + '<div class="meter"><div class="meter-top"><b class="count" data-count="' + Math.round(row.points) + '">' + format.points(row.points) + '</b>'
            + '<span class="of">' + (top ? tx('dossier.bond') : tx('dossier.bond.of', { points: format.points(next) })) + '</span>'
            + '<em class="next">' + (top ? tx('dossier.complete') : tx('dossier.until', { points: format.points(tiers.untilNext(row.points)), tier: tiers.TIERS[index + 1].name })) + '</em></div>'
            + '<div class="vial"><span class="fill" style="width:' + pct(Math.max(.04, tiers.tierProgress(row.points))) + '"><span class="sheen"></span></span></div>'
            + gateHtml(row.gate) + '</div></div></section>';

        const values = '<section class="card glass values-card"><h3 class="eyebrow">' + tx('values.title') + '</h3><div class="values">'
            + '<div class="col"><h4 class="deepens">' + tx('common.deepens') + '</h4><ul class="list scroller" tabindex="0" data-ml-id="body-likes">' + listHtml(row.likes, row.favorites, 1, options, tx('values.none.likes')) + '</ul></div>'
            + '<div class="col"><h4 class="wound">' + tx('common.wounds') + '</h4><ul class="list scroller" tabindex="0" data-ml-id="body-dislikes">' + listHtml(row.dislikes, row.petPeeves, -1, options, tx('values.none.dislikes')) + '</ul></div>'
            + '</div></section>';

        const recent = '<section class="card glass recent-card"><h3 class="eyebrow">' + tx('recent.title') + '</h3><ul class="recent scroller" tabindex="0" data-ml-id="body-recent">'
            + (row.recent.length ? row.recent.map(change => {
                const points = Number(change && change.points) || 0;
                return '<li class="change"><b class="delta' + (points < 0 ? ' minus' : '') + '">' + format.signed(points) + '</b>'
                    + '<span class="text"><span class="label">' + esc(change && change.label) + '</span><time class="when">' + esc(change && change.when) + '</time></span></li>';
            }).join('') : '<li class="none">' + tx('recent.none') + '</li>')
            + '</ul><button class="tool journal-open" data-action="journal.open" data-target="' + identity + '" data-ml-id="journal-open">' + tx('recent.journal') + '</button></section>';

        let persona = '<section class="card glass persona-card"><h3 class="eyebrow">' + tx('persona.title') + '</h3>';
        const [title, line] = origin(row);
        persona += '<div class="body scroller" tabindex="0" data-ml-id="body-persona">'
            + '<strong class="origin">' + esc(title) + '</strong><span class="seal">' + esc(line) + '</span>'
            + '<div class="meta">'
            + (row.sourcePlugin ? '<div class="line"><span class="key">' + tx('persona.plugin') + '</span><span class="value">' + esc(row.sourcePlugin) + '</span></div>' : '')
            + (row.profileFile ? '<div class="line"><span class="key">' + tx('persona.file') + '</span><span class="value">' + esc(row.profileFile) + '</span></div>' : '')
            + '</div></div></section>';

        const steps = tiers.TIERS.map((t, i) => '<div class="step' + (i <= index ? ' reached' : '') + (i === index ? ' here' : '') + '">'
            + '<span class="medal">' + icon(t.key) + '</span><em class="tname' + (i === index ? ' foil' : '') + '">' + esc(t.name) + '</em>'
            + '<small class="at">' + format.points(t.at) + '</small></div>').join('');
        // One row on top: the heading (it gives way with an ellipsis; its title has it whole), then without tools the unlock
        // lines on their own, with tools both in one head.
        const head = tools ? '<div class="mhead">' + unlocksHtml(row, index) + '<div class="tools">' + tools + '</div></div>' : unlocksHtml(row, index);
        const miles = '<section class="miles glass"><div class="mtop"><h3 class="eyebrow" title="' + tx('miles.title') + '"><span class="htext">'
            + tx('miles.title') + '</span></h3>' + head + '</div>'
            + '<div class="thread"><span class="fill" style="transform:scaleX(' + (Math.round(thread * 1000) / 1000) + ')"></span></div>'
            + '<div class="steps">' + steps + '</div></section>';

        return hero + '<div class="mid">' + values + recent + persona + '</div>' + miles;
    }

    // view: with find text that matched no one, the pane says so (the roster says it too) and offers nothing.
    function emptyHtml(state, view) {
        if (nobodyFound(state, view)) {
            return '<div class="empty"><div class="emptyheart"><span class="glow"></span>' + icon('empty-ledger', 'art book') + '</div>'
                + '<p class="line">' + tx('roster.nobody') + '</p></div>';
        }
        const away = state.romances.length > 0;
        return '<div class="empty">'
            + '<div class="emptyheart"><span class="glow"></span>' + icon('empty-ledger', 'art book') + '</div>'
            + '<p class="line">' + tx(away ? 'empty.away' : 'empty.none') + '</p>'
            + (away
                ? '<button class="btn" data-action="settings">' + icon('settings') + tx('empty.settings') + '</button>'
                : '<button class="btn primary" data-action="new-bond">' + icon('newbond') + tx('common.newbond') + '</button>')
            + '</div>';
    }

    // The selected row among the shown ones, or null when none are shown.
    function selectedRow(state, view) {
        const rows = rules.shownRows(state, view);
        const found = rows.find(row => model.rowIdentity(row) === view.selected);
        if (found) return found;
        const index = Number.isFinite(view.selectedIndex) ? view.selectedIndex : 0;
        const identity = model.reconcileSelection(view.selected, index, rows);
        return identity === null ? null : rows.find(row => model.rowIdentity(row) === identity) || null;
    }

    const api = { rosterHtml, dossierHtml, emptyHtml, selectedRow };
    if (node) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.ledger = api;

    /* ---------- DOM ---------- */
    const RM = window.RM;
    const emit = (name, payload) => RM.app.emit(name, payload);
    let rosterPainted = null, panePainted = '', paneIdentity = null, booted = false;

    // Count the points up from zero the first time a bond's dossier opens. Under Reduce motion the final number
    // shows at once (the markup already holds it).
    function countUp(root, reduced) {
        const el = root.querySelector('[data-count]');
        if (!el || reduced) return;
        const target = Number(el.getAttribute('data-count')) || 0, started = Date.now();
        // Timers rather than animation frames: a frame may never come while the view is hidden.
        const tick = () => {
            if (!el.isConnected) return;
            const t = Math.min(1, (Date.now() - started) / 900), eased = 1 - Math.pow(1 - t, 3);
            el.textContent = format.points(Math.round(target * eased));
            if (t < 1) setTimeout(tick, 16);
        };
        el.textContent = '0';
        setTimeout(tick, 16);
    }

    // The first paint draws the whole roster; after that each part is repainted only when it changed, and the head
    // never is: its find field keeps the focus and what the player typed.
    function paintRoster(state, view) {
        const roster = document.getElementById('roster');
        if (!roster) return;
        const parts = { company: companyHtml(state), rows: groupsHtml(state, view), rail: railHtml(state) };
        if (!rosterPainted || !roster.querySelector('.findbox')) {
            roster.innerHTML = rosterHtml(state, view);
        } else {
            for (const key of ['company', 'rows', 'rail']) {
                const el = roster.querySelector('.' + key);
                if (!el || parts[key] === rosterPainted[key]) continue;
                const scroll = el.scrollTop;
                el.innerHTML = parts[key];
                el.scrollTop = scroll;
            }
        }
        rosterPainted = parts;
        const find = findOf(view), box = roster.querySelector('.findbox'), clear = roster.querySelector('.clear');
        if (box && box.value !== find) box.value = find;
        if (clear) clear.disabled = find === '';
        for (const el of roster.querySelectorAll('[role="radio"]')) {
            el.setAttribute('aria-checked', el.getAttribute('data-value') === state.rosterOrder ? 'true' : 'false');
        }
        if (!booted) {
            booted = true;
            roster.classList.add('enter');
            setTimeout(() => roster.classList.remove('enter'), 1400);
        }
    }

    // Brings the selected row into view inside the rows list, by the least that shows it.
    function revealSelected() {
        const list = document.querySelector('#roster .rows'), selected = list && list.querySelector('.row.selected');
        if (!selected) return;
        if (selected.offsetTop < list.scrollTop) list.scrollTop = selected.offsetTop;
        else if (selected.offsetTop + selected.offsetHeight > list.scrollTop + list.clientHeight) {
            list.scrollTop = selected.offsetTop + selected.offsetHeight - list.clientHeight;
        }
    }

    function paintPane(state, view) {
        const pane = document.getElementById('pane');
        if (!pane) return;
        const row = selectedRow(state, view);
        const identity = row ? model.rowIdentity(row) : null;
        const html = row ? dossierHtml(row, state.preferenceOptions) : emptyHtml(state, view);
        const fresh = identity !== paneIdentity || panePainted === '';
        if (html === panePainted && !fresh) return;
        // The same bond repainted (points ticking, a refresh): each card body keeps where the player scrolled it.
        const kept = fresh ? [] : [...pane.querySelectorAll('[data-ml-id]')].map(el => [el.getAttribute('data-ml-id'), el.scrollTop]);
        pane.className = row ? 'dossier' : 'glass blank';
        pane.innerHTML = html;
        for (const [id, top] of kept) {
            const el = top ? [...pane.querySelectorAll('[data-ml-id]')].find(n => n.getAttribute('data-ml-id') === id) : null;
            if (el) el.scrollTop = top;
        }
        panePainted = html;
        paneIdentity = identity;
        if (row && fresh) {
            void pane.offsetWidth;   // restart the entrance
            pane.classList.add('enter');
            countUp(pane, state.reduceMotion === true);
            revealSelected();
        }
    }

    function render(state, view) {
        paintRoster(state, view);
        if (view.pane === 'dossier') paintPane(state, view);
        else { panePainted = ''; paneIdentity = null; }
    }

    // New find text: the rows are arranged afresh and the selection follows the rule in roster.js. A selection
    // moved by the filter closes nothing and asks the plugin for nothing, except that a Journal whose bond no
    // longer shows gives way to the dossier of the newly selected row.
    function setFind(text) {
        const app = RM.app, view = app.view;
        if (findOf(view) === text) return;
        view.find = text;
        const order = rules.settle(app.state, view).order;
        const next = rules.nextSelection(view.selected, order);
        if (next !== view.selected) {
            if (view.pane === 'journal') view.pane = 'dossier';
            view.selected = next;
        }
        if (next !== null) view.selectedIndex = order.indexOf(next);
        app.render();
        const list = document.querySelector('#roster .rows');
        if (list) list.scrollTop = 0;
        revealSelected();
    }

    function onInput(event) {
        const el = event.target;
        if (el && el.classList && el.classList.contains('findbox')) setFind(String(el.value || ''));
    }

    const target = el => el.getAttribute('data-target');
    const actions = {
        'select': el => emit('select', el.getAttribute('data-identity')),
        'new-bond': () => emit('open-enroll', { mode: 'create' }),
        'settings': () => emit('open-settings'),
        'refresh': () => emit('refresh'),
        'close': () => emit('close'),
        'edit': el => emit('open-enroll', { mode: 'edit', identity: target(el) }),
        'reset': el => emit('confirm', { kind: 'reset', identity: target(el) }),
        'remove': el => emit('confirm', { kind: 'remove', identity: target(el) }),
        // The clear button goes off once it has cleared, so the focus moves on: to the find field after a clear by the
        // mouse or the keys (typing again is the natural next step), to the selected row after one by the gamepad,
        // which never lands on the text field.
        'roster.clear': () => {
            const roster = document.getElementById('roster'), box = roster && roster.querySelector('.findbox');
            if (box) box.value = '';
            setFind('');
            const pad = document.documentElement.getAttribute('data-ml-input') === 'gamepad';
            const to = pad ? roster && (roster.querySelector('.row.selected') || roster.querySelector('.row')) : box;
            if (to) to.focus({ preventScroll: true });
        },
        // An order sends its one key; the current one sends nothing. The state that answers re-arranges the rows.
        'roster.order': el => {
            const value = el ? el.getAttribute('data-value') : null;
            if (!ORDERS.some(o => o[0] === value) || RM.app.state.rosterOrder === value) return;
            RM.bridge.send('romantasySettings', { rosterOrder: value });
        }
    };

    function init() {
        const roster = document.getElementById('roster');
        if (roster) roster.addEventListener('input', onInput);
    }

    (RM.parts = RM.parts || []).push({ name: 'ledger', render, actions, init });
})();
