// The Journal pane's pure parts: the answer's normaliser and the markup. The flow (open, answer, wait, Back,
// keys and the pad) is in InputFlow.mjs; the mock's answer in Mock.mjs.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const j = require('../../view/js/journal.js');

const row = { name: 'Aelira', levelName: 'Friend' };
const ANSWER = {
    identity: '00ABCDEF', found: true, began: 3, daysTogether: 12.46,
    milestones: [
        { kind: 'began', level: 1, day: 3 },
        { kind: 'reached', level: 2, day: 5 },
        { kind: 'reached', level: 3, day: 11 },
        { kind: 'fell', level: 2, day: 13 }
    ],
    days: [
        { day: 3, gained: 40, lost: 0 },
        { day: 5, gained: 0, lost: 20 },
        { day: 11, gained: 80, lost: 10 }
    ],
    gains: [{ label: 'Cleared a dungeon', points: 25, day: 11 }, { label: 'Quest done', points: 10, day: 3 }],
    losses: [{ label: 'Stole something', points: -20, day: 5 }],
    history: [
        { label: 'Stole something', points: -20, day: 5 },
        { label: 'Cleared a dungeon', points: 25, day: 4 },
        { label: 'Quest done', points: 10, day: 3 }
    ]
};
const html = j.journalHtml(row, ANSWER);
const text = h => h.replace(/<[^>]*>/g, '\n');

/* ---------- the words, verbatim ---------- */
for (const heading of ['Milestones', 'The last sixty days', 'Biggest moments', 'Deepened most', 'Wounded most', 'History']) {
    assert.ok(text(html).split('\n').includes(heading), 'the heading ' + heading);
}
assert.match(html, /data-action="journal\.back"[^>]*>.*Back<\/button>/, 'a Back button');
assert.match(html, /<h2 class="jname">Aelira<\/h2>/, 'the companion\'s name');
assert.match(html, /<em class="jtier foil">Friend<\/em>/, 'and tier');
assert.match(html, /Since Day 3 · 12\.5 days together/, 'the journal\'s first day and the days together');

/* ---------- milestones, oldest first ---------- */
const miles = [...html.matchAll(/<li class="jmile (\w+)">([^<]*)<\/li>/g)].map(m => [m[1], m[2]]);
assert.deepEqual(miles, [['began', 'Day 3 · Journal began'], ['reached', 'Day 5 · Reached Acquaintance'],
    ['reached', 'Day 11 · Reached Friend'], ['fell', 'Day 13 · Fell to Acquaintance']]);

/* ---------- the chart: one bar per day, sized against the largest value ---------- */
const bars = [...html.matchAll(/<span class="jday" title="([^"]*)" data-tip="\1"><span class="jbar"><span class="up"><span class="gain[^"]*" style="height:([\d.]+)%"><\/span><\/span><span class="down"><span class="loss[^"]*" style="height:([\d.]+)%"><\/span><\/span><\/span><\/span>/g)]
    .map(m => [m[1], Number(m[2]), Number(m[3])]);
assert.deepEqual(bars, [['Day 3: +40, −0', 50, 0], ['Day 5: +0, −20', 0, 25], ['Day 11: +80, −10', 100, 12.5]]);
// The page draws each day's numbers itself on hover (the host may draw no native tooltip): data-tip carries the title's text.
const tips = h => [...h.matchAll(/data-tip="([^"]*)"/g)].map(m => m[1]);
assert.deepEqual(tips(html), ['Day 3: +40, −0', 'Day 5: +0, −20', 'Day 11: +80, −10'], 'a gain-only day, a loss-only day and a mixed day');
assert.deepEqual(tips(j.journalHtml(row, { ...ANSWER, days: [{ day: 4, gained: 1234, lost: 56789 }] })), ['Day 4: +1,234, −56,789'], 'grouped digits');
// Hostile numbers never reach the attribute as they came: only normalised, formatted numbers do.
const hostileDays = j.journalHtml(row, { ...ANSWER, days: [
    { day: '"><img src=x>', gained: 5, lost: 1 },
    { day: 9, gained: '"><b>x</b>', lost: { toString: () => '<i>' } },
    { day: 10.9, gained: 2.6, lost: -7 },
    { day: 11, gained: NaN, lost: Infinity }
] });
assert.deepEqual(tips(hostileDays), ['Day 9: +0, −0', 'Day 10: +3, −0', 'Day 11: +0, −0']);
for (const t of tips(hostileDays)) assert.match(t, /^Day \d+: \+[\d,]+, −[\d,]+$/, 'only digits in a tip: ' + t);
assert.doesNotMatch(hostileDays, /<img|<b>|<i>|NaN|Infinity/);
assert.match(html, /Gained 120 · Lost 30/, 'the totals line');
assert.match(html, /<span class="gain" style="height:0%">/, 'a day with no gain has an empty gain bar (no minimum height)');
assert.match(html, /<span class="loss some" style="height:25%">/, 'a value shows at least a sliver');
// One day; and every value 0: no division by zero.
const one = j.journalHtml(row, { ...ANSWER, days: [{ day: 7, gained: 5, lost: 0 }] });
assert.match(one, /title="Day 7: \+5, −0" data-tip="Day 7: \+5, −0"><span class="jbar"><span class="up"><span class="gain some" style="height:100%">/);
assert.equal((one.match(/class="jday"/g) || []).length, 1);
assert.match(one, /<div class="jaxis"><span>Day 7<\/span><\/div>/, 'one day: one axis label');
const zero = j.journalHtml(row, { ...ANSWER, days: [{ day: 7, gained: 0, lost: 0 }, { day: 8, gained: 0, lost: 0 }] });
assert.doesNotMatch(zero, /NaN|Infinity/);
assert.equal((zero.match(/style="height:0%"/g) || []).length, 4);
assert.match(zero, /Gained 0 · Lost 0/);
// Sixty days: sixty bars, the axis from the first to the last.
const sixty = j.journalHtml(row, { ...ANSWER, days: Array.from({ length: 60 }, (_, i) => ({ day: i * 2 + 1, gained: i, lost: 60 - i })) });
assert.equal((sixty.match(/class="jday"/g) || []).length, 60);
assert.match(sixty, /<span>Day 1<\/span><span>Day 119<\/span>/);
assert.doesNotMatch(sixty, /height:(1[0-9]{2}\.|[2-9][0-9]{2})/, 'no bar above 100%');

/* ---------- biggest moments and history ---------- */
const lists = [...html.matchAll(/<ul class="jlist">([\s\S]*?)<\/ul>/g)].map(m => [...m[1].matchAll(/<b class="delta( minus)?">([^<]*)<\/b><span class="text"><span class="label">([^<]*)<\/span><span class="when">([^<]*)<\/span>/g)]
    .map(e => [e[2], e[3], e[4]]));
assert.deepEqual(lists[0], [['+25', 'Cleared a dungeon', 'Day 11'], ['+10', 'Quest done', 'Day 3']], 'Deepened most');
assert.deepEqual(lists[1], [['−20', 'Stole something', 'Day 5']], 'Wounded most');
assert.deepEqual(lists[2], [['−20', 'Stole something', 'Day 5'], ['+25', 'Cleared a dungeon', 'Day 4'], ['+10', 'Quest done', 'Day 3']], 'History, newest first');
assert.match(html, /<b class="delta minus">−20/, 'a loss is marked');
// One side empty: the other still shows.
const noLoss = j.journalHtml(row, { ...ANSWER, losses: [] });
assert.match(noLoss, /Wounded most<\/h4><ul class="jlist"><li class="none">None yet\.<\/li>/);

/* ---------- waiting, failure, empty ---------- */
const bodyOf = h => h.slice(h.indexOf('<div class="jbody'));
assert.match(bodyOf(j.journalHtml(row, null)), /<p class="jnote">Opening the journal…<\/p><\/div>$/);
assert.match(bodyOf(j.journalHtml(row, { identity: '00ABCDEF', found: false })), /<p class="jnote">The journal could not be opened\.<\/p><\/div>$/);
for (const bad of ['nope', 42, [], true]) assert.match(j.journalHtml(row, bad), /The journal could not be opened\./, JSON.stringify(bad) + ' is a failure');
const empty = j.journalHtml(row, { identity: '00ABCDEF', found: true, began: 0, daysTogether: 0, milestones: [], days: [], gains: [], losses: [], history: [] });
assert.match(bodyOf(empty), /<p class="jnote">Nothing is written here yet\.<\/p><\/div>$/);
assert.doesNotMatch(empty, /Milestones|History/);
assert.doesNotMatch(j.journalHtml(row, null), /Since Day|together/, 'no facts before the answer');
// The pane's one body scrolls, and keys and the pad can reach it.
for (const h of [html, empty, j.journalHtml(row, null)]) assert.match(h, /<div class="jbody scroller" tabindex="0" data-ml-id="body-journal">/);

/* ---------- text is text ---------- */
const hostile = j.journalHtml({ name: '<i>n</i>', levelName: '<u>t</u>' }, {
    ...ANSWER,
    gains: [{ label: '<b>x</b>', points: 5, day: 1 }], losses: [{ label: '"><img src=x>', points: -5, day: 1 }],
    history: [{ label: '<script>alert(1)</script>', points: 1, day: 1 }]
});
assert.match(hostile, /&lt;b&gt;x&lt;\/b&gt;/);
assert.doesNotMatch(hostile, /<b>x<\/b>|<i>n<\/i>|<u>t<\/u>|<img|<script/, 'no label, name or tier becomes markup');

/* ---------- the normaliser ---------- */
const n = j.normalize;
for (const bad of [null, undefined, 'x', 3, [], true]) assert.equal(n(bad), null, JSON.stringify(bad) + ' is no answer');
assert.deepEqual(n({ identity: 'A', found: false, milestones: [{ kind: 'began', level: 1, day: 1 }] }),
    { identity: 'A', found: false, began: 0, daysTogether: 0, milestones: [], days: [], gains: [], losses: [], history: [] }, 'not found carries no lists');
assert.equal(n({ found: 'yes' }).found, false, 'found must be true');
assert.equal(n({ identity: 7, found: true }).identity, '', 'an identity is a string');
const good = n(ANSWER);
assert.deepEqual(n(good), good, 'it is idempotent');
assert.deepEqual(Object.keys(n({ ...ANSWER, extra: '<x>' })).sort(),
    ['began', 'days', 'daysTogether', 'found', 'gains', 'history', 'identity', 'losses', 'milestones'], 'only the known fields');
// Numbers: finite, days whole and at least 1, sizes not negative.
const odd = n({
    identity: 'A', found: true, began: NaN, daysTogether: -4,
    milestones: [{ kind: 'reached', level: 7, day: 2 }, { kind: 'reached', level: 0, day: 2 }, { kind: 'reached', level: 2.5, day: 2 },
        { kind: 'wed', level: 2, day: 2 }, { kind: 'fell', level: '2', day: 2 }, { kind: 'fell', level: 2, day: Infinity }, 'x', null,
        { kind: 'fell', level: 2, day: -3 }, { kind: 'reached', level: 6, day: 9.7 }],
    days: [{ day: 'x' }, { day: 2, gained: -5, lost: NaN }, { day: 3.9, gained: 1e400, lost: 4.6 }, null, []],
    gains: [{ label: 5, points: 3, day: 1 }, { label: 'a', points: 'x', day: 1 }, { label: 'b', points: 2, day: NaN }, { label: 'c', points: 2.4, day: 0 }],
    losses: 'nope', history: { 0: { label: 'x', points: 1, day: 1 } }
});
assert.equal(odd.began, 0);
assert.equal(odd.daysTogether, 0);
assert.deepEqual(odd.milestones, [{ kind: 'fell', level: 2, day: 1 }, { kind: 'reached', level: 6, day: 9 }]);
assert.deepEqual(odd.days, [{ day: 2, gained: 0, lost: 0 }, { day: 3, gained: 0, lost: 5 }]);
assert.deepEqual(odd.gains, [{ label: '', points: 3, day: 1 }, { label: 'c', points: 2, day: 1 }]);
assert.deepEqual(odd.losses, []);
assert.deepEqual(odd.history, []);
assert.doesNotMatch(j.journalHtml(row, odd), /NaN|Infinity|undefined/);
// Lists cut to the plugin's limits: the first of each, except the days, which keep the newest.
const many = (k, f) => Array.from({ length: k }, (_, i) => f(i));
const big = n({
    identity: 'A', found: true,
    milestones: many(40, i => ({ kind: 'fell', level: 1 + (i % 6), day: i + 1 })),
    days: many(80, i => ({ day: i + 1, gained: 1, lost: 1 })),
    gains: many(9, i => ({ label: 'g' + i, points: 9 - i, day: 1 })),
    losses: many(9, i => ({ label: 'l' + i, points: i - 9, day: 1 })),
    history: many(50, i => ({ label: 'h' + i, points: 1, day: 1 }))
});
// 27 milestones: the plugin's 26 plus "A first moment together".
assert.deepEqual([big.milestones.length, big.days.length, big.gains.length, big.losses.length, big.history.length], [27, 60, 5, 5, 30]);
assert.deepEqual([j.LIMITS.milestones, j.LIMITS.days, j.LIMITS.moments, j.LIMITS.history], [27, 60, 5, 30]);
assert.equal(big.days[0].day, 21, 'the oldest days fall off');
assert.equal(big.history[0].label, 'h0', 'the newest history rows stay');
// Moments keep their sign: "Deepened most" holds gains above 0, "Wounded most" losses below 0; the rest are dropped
// before the cut to five, so five good ones after bad ones all stay.
const signed = n({
    identity: 'A', found: true,
    gains: [{ label: 'zero', points: 0, day: 1 }, { label: 'neg', points: -4, day: 1 }, { label: 'rounds to 0', points: 0.4, day: 1 },
        ...many(5, i => ({ label: 'g' + i, points: 10 - i, day: 1 }))],
    losses: [{ label: 'zero', points: 0, day: 1 }, { label: 'pos', points: 4, day: 1 }, { label: 'rounds to 0', points: -0.4, day: 1 },
        ...many(5, i => ({ label: 'l' + i, points: i - 10, day: 1 }))],
    history: [{ label: 'zero', points: 0, day: 1 }, { label: 'neg', points: -4, day: 1 }, { label: 'pos', points: 4, day: 1 }]
});
assert.deepEqual(signed.gains.map(c => c.label), ['g0', 'g1', 'g2', 'g3', 'g4'], 'gains of 0 or less are dropped');
assert.deepEqual(signed.losses.map(c => c.label), ['l0', 'l1', 'l2', 'l3', 'l4'], 'losses of 0 or more are dropped');
assert.deepEqual(signed.history.map(c => c.label), ['zero', 'neg', 'pos'], 'history keeps every sign');
const flipped = j.journalHtml(row, { ...ANSWER, gains: [{ label: 'Wrong side', points: -9, day: 1 }], losses: [{ label: 'Also wrong', points: 9, day: 1 }] });
assert.doesNotMatch(flipped, /Wrong side|Also wrong/);
assert.doesNotMatch(flipped, /Biggest moments/, 'with only wrong-signed moments the section is left out');
// A label longer than the plugin would send is cut to 120 characters.
assert.equal(n({ found: true, history: [{ label: 'x'.repeat(500), points: 1, day: 1 }] }).history[0].label.length, 120);
assert.equal(j.WAIT_MS, 3000);

/* ---------- the first moment together ---------- */
// The plugin places it among the milestones by day; the page shows it in the same line as the others.
const shared = j.journalHtml(row, { ...ANSWER, milestones: [
    { kind: 'began', level: 1, day: 3 }, { kind: 'shared', level: 0, day: 4 }, { kind: 'reached', level: 2, day: 5 }] });
const sharedMiles = [...shared.matchAll(/<li class="jmile (\w+)">([^<]*)<\/li>/g)].map(m => [m[1], m[2]]);
assert.deepEqual(sharedMiles, [['began', 'Day 3 · Journal began'], ['shared', 'Day 4 · A first moment together'],
    ['reached', 'Day 5 · Reached Acquaintance']]);
// "shared" carries level 0 only; the other kinds keep 1 to 6.
assert.deepEqual(n({ identity: 'A', found: true, milestones: [
    { kind: 'shared', level: 3, day: 4 }, { kind: 'reached', level: 0, day: 4 }, { kind: 'shared', level: '0', day: 4 },
    { kind: 'shared', level: 0, day: NaN }, { kind: 'shared', level: 0, day: 4.6 }, { kind: 'began', level: 0, day: 1 }] }).milestones,
    [{ kind: 'shared', level: 0, day: 4 }]);
assert.doesNotMatch(j.journalHtml(row, { ...ANSWER, milestones: [{ kind: 'shared', level: 3, day: 4 }, { kind: 'reached', level: 0, day: 4 }] }),
    /first moment|jmile/, 'a shared milestone with a level, or a reached one with none, is dropped');

console.log('Journal: ok');
