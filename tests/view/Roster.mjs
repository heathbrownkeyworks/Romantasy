// The roster's rules: groups, the three orders, find, the pinned arrangement and the selection after a filter.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const s = require('../../view/js/state.js');
const r = require('../../view/js/roster.js');

const row = (name, points, isFollowing, recent, id) => ({ name, points, isFollowing, recent: recent || [], referenceFormID: id || name, baseFormID: '' });
const ROWS = s.normalize({ romances: [
    row('Mirelle', 2630, true, [{ label: 'a', points: 1, when: 'Yesterday' }]),
    row('aelira', 2240, true, [{ label: 'a', points: 1, when: '3 days ago' }]),
    row('Lydia', 1120, true),
    row('Bryn', 1120, true, [{ label: 'a', points: 1, when: 'Today' }]),
    row('Inalion', 940, false, [{ label: 'a', points: 1, when: '5 days ago' }]),
    row('Ravonna', 1315, false),
    row('Kaela', 80, false, [{ label: 'a', points: 1, when: 'Today' }])
] }).romances;
const names = list => list.map(x => x.name);
const ids = arranged => arranged.order;
const groupOf = (a, key) => (a.groups.find(g => g.key === key) || { rows: [] }).rows;

/* ---------- groups and counts ---------- */
{
    const a = r.arrange(ROWS, { order: 'closest', showAway: true, find: '', pinned: null });
    assert.deepEqual(a.groups.map(g => [g.key, g.title, g.rows.length]), [['following', 'Following', 4], ['away', 'Away', 3]]);
    assert.deepEqual(a.order, names(groupOf(a, 'following')).concat(names(groupOf(a, 'away'))), 'the order runs through Following, then Away');
    const hidden = r.arrange(ROWS, { order: 'closest', showAway: false, find: '', pinned: null });
    assert.deepEqual(hidden.groups.map(g => g.key), ['following'], 'no Away group while away companions are hidden');
    const allHere = r.arrange(ROWS.filter(x => x.isFollowing), { order: 'closest', showAway: true, find: '', pinned: null });
    assert.deepEqual(allHere.groups.map(g => g.key), ['following'], 'no Away group when nobody is away');
    const allAway = r.arrange(ROWS.filter(x => !x.isFollowing), { order: 'closest', showAway: true, find: '', pinned: null });
    assert.deepEqual(allAway.groups.map(g => g.key), ['away'], 'an empty Following group shows no heading either');
    assert.deepEqual(r.arrange([], { order: 'closest', showAway: true }), { groups: [], order: [] });
}

/* ---------- each order, inside each group ---------- */
{
    const by = order => r.arrange(ROWS, { order, showAway: true, find: '', pinned: null });
    assert.deepEqual(names(groupOf(by('closest'), 'following')), ['Mirelle', 'aelira', 'Bryn', 'Lydia'], 'closest: most points first, ties by name');
    assert.deepEqual(names(groupOf(by('closest'), 'away')), ['Ravonna', 'Inalion', 'Kaela']);
    assert.deepEqual(names(groupOf(by('name'), 'following')), ['aelira', 'Bryn', 'Lydia', 'Mirelle'], 'name: A to Z, ignoring case');
    assert.deepEqual(names(groupOf(by('name'), 'away')), ['Inalion', 'Kaela', 'Ravonna']);
    assert.deepEqual(names(groupOf(by('recent'), 'following')), ['Bryn', 'Mirelle', 'aelira', 'Lydia'], 'recent: the newest change first, then the rest by closest');
    assert.deepEqual(names(groupOf(by('recent'), 'away')), ['Kaela', 'Inalion', 'Ravonna']);
    // Equally new changes keep the Closest order.
    const same = s.normalize({ romances: [row('Zed', 10, true, [{ label: 'a', points: 1, when: 'Today' }]), row('Abe', 900, true, [{ label: 'a', points: 1, when: 'Today' }])] }).romances;
    assert.deepEqual(names(r.arrange(same, { order: 'recent', showAway: true }).groups[0].rows), ['Abe', 'Zed']);
    // A change with no day comes after the dated ones and before the rows with none.
    const undated = s.normalize({ romances: [row('Nil', 5, true), row('Odd', 1, true, [{ label: 'a', points: 0 }]), row('Day', 0, true, [{ label: 'a', points: 1, when: '40 days ago' }])] }).romances;
    assert.deepEqual(names(r.arrange(undated, { order: 'recent', showAway: true }).groups[0].rows), ['Day', 'Odd', 'Nil']);
}

/* ---------- Recent by the day of the change; the day words only when there is no day number ---------- */
{
    const change = (when, day) => [{ label: 'a', points: 1, when, day }];
    const recent = list => names(r.arrange(s.normalize({ romances: list }).romances, { order: 'recent', showAway: true }).groups[0].rows);
    // Two bonds changed "Today": the later day number first, whatever their points.
    assert.deepEqual(recent([row('Early', 900, true, change('Today', 120.2)), row('Late', 10, true, change('Today', 120.7))]), ['Late', 'Early']);
    assert.deepEqual(recent([row('Late', 10, true, change('Today', 120.7)), row('Early', 900, true, change('Today', 120.2))]), ['Late', 'Early'], 'in either state order');
    // Words only: still by the words.
    assert.deepEqual(recent([row('Old', 900, true, change('3 days ago')), row('New', 10, true, change('Yesterday'))]), ['New', 'Old']);
    // A day number sorts before words only for the same day, and after newer words.
    assert.deepEqual(recent([row('Words', 900, true, change('Today')), row('Number', 10, true, change('Today', 120.1))]), ['Number', 'Words']);
    assert.deepEqual(recent([row('Number', 900, true, change('Yesterday', 119.9)), row('Words', 10, true, change('Today'))]), ['Words', 'Number']);
    assert.deepEqual(recent([row('Words', 900, true, change('2 days ago')), row('Number', 10, true, change('Yesterday', 119.9))]), ['Number', 'Words']);
    // Both rows with a day number: the number decides, before the words (here the words disagree with it).
    assert.deepEqual(recent([row('WordsNewer', 900, true, change('Today', 120.5)), row('NumberNewer', 10, true, change('Yesterday', 130.5))]),
        ['NumberNewer', 'WordsNewer'], 'the day number comes first when both rows have it');
    // Only one row (or neither) with a day number: the words are the fallback.
    assert.deepEqual(recent([row('Number', 900, true, change('3 days ago', 130.5)), row('Words', 10, true, change('Today'))]), ['Words', 'Number'],
        'with one day number, the words decide');
    // A hostile day is no day: the row sorts by its words, beside a row with none; nothing throws.
    for (const bad of [NaN, Infinity, -Infinity, '120.9', -3, null, {}]) {
        const raw = [row('Bad', 900, true, change('Today', bad)), row('Words', 10, true, change('Today')), row('Good', 1, true, change('Today', 120.1))];
        assert.deepEqual(recent(raw), ['Good', 'Bad', 'Words'], 'normalised: ' + String(bad));
        // Straight from the plugin, without normalising, it is still no day.
        assert.deepEqual(names(r.arrange(raw, { order: 'recent', showAway: true }).groups[0].rows), ['Good', 'Bad', 'Words'], 'raw: ' + String(bad));
    }
}

/* ---------- find: literal, ignoring case, trimmed ---------- */
assert.equal(r.matches('Aelira', ''), true, 'an empty find matches all');
assert.equal(r.matches('Aelira', '   '), true);
assert.equal(r.matches('Aelira', ' LIR '), true);
assert.equal(r.matches('Aelira', 'lyd'), false);
assert.equal(r.matches('', 'a'), false);
{
    const a = r.arrange(ROWS, { order: 'closest', showAway: true, find: 'EL', pinned: null });
    assert.deepEqual(a.groups.map(g => [g.key, names(g.rows)]), [['following', ['Mirelle', 'aelira']], ['away', ['Kaela']]]);
    const none = r.arrange(ROWS, { order: 'closest', showAway: true, find: 'zzz', pinned: null });
    assert.deepEqual(none, { groups: [], order: [] }, 'no match: no group, no row');
}

/* ---------- review focus 3: regex characters and non-Latin letters match literally ---------- */
{
    const odd = s.normalize({ romances: [row('Серана <b>', 1, true), row('J(ack)*', 2, true), row('Back\\slash', 3, true), row('Jack', 4, true)] }).romances;
    const find = f => names(r.arrange(odd, { order: 'name', showAway: true, find: f }).order.map(id => odd.find(x => s.rowIdentity(x) === id)));
    assert.deepEqual(find('сер'), ['Серана <b>'], 'Cyrillic, ignoring case');
    assert.deepEqual(find('СЕРАНА <B>'), ['Серана <b>']);
    assert.deepEqual(find('('), ['J(ack)*'], 'a parenthesis is a parenthesis');
    assert.deepEqual(find('*'), ['J(ack)*'], 'a star is a star');
    assert.deepEqual(find('\\'), ['Back\\slash'], 'a backslash is a backslash');
    assert.deepEqual(find('.'), [], 'a dot matches only a dot');
    assert.deepEqual(find('[a-z]'), [], 'a class is text');
}

/* ---------- review focus 1: a pinned arrangement keeps every place; a new row goes last in its group ---------- */
{
    const first = r.arrange(ROWS, { order: 'closest', showAway: true, find: '', pinned: null });
    // Lydia climbs past everyone and Mirelle falls to nothing; a newcomer arrives in each group.
    const moved = ROWS.map(x => x.name === 'Lydia' ? { ...x, points: 9000 } : x.name === 'Mirelle' ? { ...x, points: 0 } : x)
        .concat(s.normalize({ romances: [row('Ann', 99999, true), row('Zoe', 1, false)] }).romances);
    const pinned = r.arrange(moved, { order: 'closest', showAway: true, find: '', pinned: first.order });
    assert.deepEqual(names(groupOf(pinned, 'following')), ['Mirelle', 'aelira', 'Bryn', 'Lydia', 'Ann'], 'points changed, places did not; the new row is last');
    assert.deepEqual(names(groupOf(pinned, 'away')), ['Ravonna', 'Inalion', 'Kaela', 'Zoe'], 'and last in its own group');
    // A row that changes group goes to the end of its new group; no other row moves.
    const flipView = s.createView();
    const st = s.normalize({ romances: ROWS.map(x => ({ ...x })), rosterOrder: 'closest' });
    r.settle(st, flipView);
    const flipped = { ...st, romances: st.romances.map(x => x.name === 'Mirelle' ? { ...x, isFollowing: false } : x.name === 'Kaela' ? { ...x, isFollowing: true } : x) };
    const after = r.settle(flipped, flipView);
    assert.deepEqual(names(groupOf(after, 'following')), ['aelira', 'Bryn', 'Lydia', 'Kaela'], 'Kaela joins Following last');
    assert.deepEqual(names(groupOf(after, 'away')), ['Ravonna', 'Inalion', 'Mirelle'], 'Mirelle joins Away last');
    // A removed row leaves the others where they were.
    const fewer = r.arrange(ROWS.filter(x => x.name !== 'aelira'), { order: 'closest', showAway: true, find: '', pinned: first.order });
    assert.deepEqual(names(groupOf(fewer, 'following')), ['Mirelle', 'Bryn', 'Lydia']);
    // Without the pin the same rows sort again.
    assert.deepEqual(names(groupOf(r.arrange(moved, { order: 'closest', showAway: true, find: '' }), 'following')), ['Ann', 'Lydia', 'aelira', 'Bryn', 'Mirelle']);

    // Through the view: a state update keeps the pin; a new order or new find text makes a fresh arrangement.
    const view = s.createView();
    const state = s.normalize({ romances: ROWS.map(x => ({ ...x })), rosterOrder: 'closest' });
    assert.deepEqual(r.settle(state, view).order, first.order);
    const later = { ...state, romances: moved };
    assert.deepEqual(names(r.shownRows(later, view)).slice(0, 5), ['Mirelle', 'aelira', 'Bryn', 'Lydia', 'Ann'], 'a state update moves no row');
    assert.deepEqual(names(r.shownRows({ ...later, rosterOrder: 'name' }, view)).slice(0, 5), ['aelira', 'Ann', 'Bryn', 'Lydia', 'Mirelle'], 'a new order re-sorts');
    view.find = 'a';
    assert.deepEqual(names(r.shownRows(later, view)), ['Ann', 'Lydia', 'aelira', 'Ravonna', 'Inalion', 'Kaela'], 'new find text re-sorts too');
    r.reset(view);
    assert.equal(view.find, '');
    assert.equal(view.pinned, null);
}

/* ---------- review focus 2: the selection through a re-order and a filter ---------- */
{
    assert.equal(r.nextSelection('B', ['A', 'B', 'C']), 'B', 'the same bond stays selected wherever it moved');
    assert.equal(r.nextSelection('X', ['A', 'B']), 'A', 'filtered out: the first shown row');
    assert.equal(r.nextSelection(null, ['A', 'B']), 'A');
    assert.equal(r.nextSelection('A', []), null, 'no match: nothing selected');
    assert.equal(r.nextSelection(null, []), null);
    const view = s.createView();
    const state = s.normalize({ romances: ROWS.map(x => ({ ...x })) });
    const sel = s.rowIdentity(ROWS[2]);
    view.find = 'ly';
    assert.equal(r.nextSelection(sel, r.settle(state, view).order), sel, 'Lydia survives "ly"');
    view.find = 'kae';
    assert.equal(r.nextSelection(sel, r.settle(state, view).order), 'Kaela', 'and gives way to the only match');
    view.find = 'nobody';
    assert.equal(r.nextSelection(sel, r.settle(state, view).order), null);
}

console.log('Roster: ok');
