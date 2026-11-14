// The roster's rules: which bonds show, in which group and in which order, and which one stays selected.
// Pure; runs under Node for the tests. ledger.js draws what arrange() returns; app.js and pad.js walk its order.
//
// Groups: Following, then Away (only while away companions are shown). A group with no row is left out.
// Order inside each group: closest (points, most first; ties by name), name (A to Z) or recent (bonds with a
// recent change first, the newest first; the rest in closest order).
// Rows do not move while the ledger is open: the order is made when the ledger opens, when the order choice
// changes and when the find text changes. Between those, the last arrangement is pinned: a row keeps its place
// and a row that was not in it (or was in the other group) goes to the end of its group.
(function () {
    'use strict';
    const node = typeof module === 'object' && module.exports;
    const model = node ? require('./state.js') : window.RM.state;
    const S = node ? require('./strings.js') : window.RM.strings;

    const GROUPS = [['following', S.t('roster.group.following')], ['away', S.t('roster.group.away')]];

    const text = v => typeof v === 'string' ? v : '';

    // Literal and ignoring case: the find text is never a pattern. Spaces around it do not count.
    function matches(name, find) {
        const f = text(find).trim().toLocaleLowerCase();
        return f === '' || text(name).toLocaleLowerCase().indexOf(f) >= 0;
    }

    // The selection after a re-order or a filter: the same bond while it shows, else the first, else none.
    function nextSelection(selected, order) {
        const list = Array.isArray(order) ? order : [];
        if (selected !== null && selected !== undefined && list.indexOf(selected) >= 0) return selected;
        return list.length ? list[0] : null;
    }

    // Fallback only, for a plugin that sends no day number: how long ago a bond's newest change was, in days, from
    // the plugin's words "Today", "Yesterday", "N days ago"; a change with no words comes after every worded one.
    function daysAgo(row) {
        const when = text(row.recent[0] && row.recent[0].when);
        if (when === 'Today') return 0;
        if (when === 'Yesterday') return 1;
        const n = /^(\d+) days ago$/.exec(when);
        return n ? Number(n[1]) : Infinity;
    }
    // The game day of a bond's newest change, or null: only a finite number of 0 or more counts.
    function changeDay(row) {
        const d = row.recent[0] && row.recent[0].day;
        return typeof d === 'number' && isFinite(d) && d >= 0 ? d : null;
    }
    const cmp = (a, b) => a < b ? -1 : a > b ? 1 : 0;

    const byName = (a, b) => text(a.name).localeCompare(text(b.name));
    const closest = (a, b) => (b.points - a.points) || byName(a, b);

    // Recent, between two rows with a change: when both carry a day number, by that number, newest first; otherwise by
    // the day words (the fallback), then a row with a day number before one with words only. Ties go in closest order.
    // A row with a day number carries its words too, so the two paths agree; no comparison can throw.
    function newest(a, b) {
        const da = changeDay(a), db = changeDay(b);
        if (da !== null && db !== null) return cmp(db, da) || closest(a, b);
        return cmp(daysAgo(a), daysAgo(b)) || cmp(da === null ? 1 : 0, db === null ? 1 : 0) || closest(a, b);
    }

    // Sorts the rows of one group. Recent: rows with a change first, the newest first; then the rest in closest order.
    function sorted(rows, order) {
        if (order === 'name') return rows.slice().sort((a, b) => byName(a, b) || closest(a, b));
        if (order === 'recent') {
            const changed = rows.filter(r => r.recent.length > 0), rest = rows.filter(r => r.recent.length === 0);
            return changed.sort(newest).concat(rest.sort(closest));
        }
        return rows.slice().sort(closest);
    }

    // rows: the state's romances. options: { order, showAway, find, pinned, pinnedGroups }. pinned is the identity
    // order of the last arrangement, or null to make a fresh one; the caller passes it only while the order and the
    // find text are the ones it was made with. pinnedGroups (optional) maps each pinned identity to the group it was
    // in: a row pinned in another group counts as new here, so a bond that changes group goes to the end of its new one.
    function arrange(rows, options) {
        const o = options || {};
        const list = Array.isArray(rows) ? rows : [];
        const pinned = Array.isArray(o.pinned) ? o.pinned : null;
        const was = o.pinnedGroups || null;
        const groups = [];
        for (const [key, title] of GROUPS) {
            if (key === 'away' && o.showAway === false) continue;
            const members = list.filter(r => (key === 'following') === (r.isFollowing === true) && matches(r.name, o.find));
            if (!members.length) continue;
            let ordered = sorted(members, o.order);
            if (pinned) {
                const at = r => {
                    const id = model.rowIdentity(r);
                    return was && was[id] !== key ? -1 : pinned.indexOf(id);
                };
                const kept = ordered.filter(r => at(r) >= 0).sort((a, b) => at(a) - at(b));
                ordered = kept.concat(ordered.filter(r => at(r) < 0));
            }
            groups.push({ key, title, rows: ordered });
        }
        const order = [];
        for (const g of groups) for (const r of g.rows) order.push(model.rowIdentity(r));
        return { groups, order };
    }

    // The view's roster fields: view.find (the find text) and view.pinned with the order and find it was made
    // with (view.pinnedBy). A view without them (a fresh one) has no find text and nothing pinned.
    const findOf = view => view && typeof view.find === 'string' ? view.find : '';
    const keyOf = (state, view) => state.rosterOrder + '\n' + findOf(view);

    // The arrangement the roster shows for this state and view.
    function arrangeFor(state, view) {
        const live = view && view.pinned && view.pinnedBy === keyOf(state, view);
        return arrange(state.romances, { order: state.rosterOrder, showAway: state.showAwayFollowers !== false, find: findOf(view),
            pinned: live ? view.pinned : null, pinnedGroups: live ? view.pinnedGroups || null : null });
    }

    // arrangeFor, then pins what it returned on the view: the order and each row's group.
    function settle(state, view) {
        const arranged = arrangeFor(state, view);
        const groups = {};
        for (const g of arranged.groups) for (const r of g.rows) groups[model.rowIdentity(r)] = g.key;
        view.pinned = arranged.order;
        view.pinnedGroups = groups;
        view.pinnedBy = keyOf(state, view);
        return arranged;
    }

    // The ledger opening: no find text, nothing pinned.
    function reset(view) {
        view.find = '';
        view.pinned = null;
        view.pinnedGroups = null;
        view.pinnedBy = null;
    }

    // The shown rows, in order.
    const shownRows = (state, view) => arrangeFor(state, view).groups.reduce((all, g) => all.concat(g.rows), []);

    const api = { GROUPS, matches, nextSelection, arrange, arrangeFor, settle, reset, shownRows };
    if (node) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.roster = api;
})();
