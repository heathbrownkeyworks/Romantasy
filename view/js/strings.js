// Every word the page shows, in one table: key on the left, text on the right. To translate the page, copy this
// file, change only the text on the right of each line, keep every {place} (the page fills it with a name or a
// number), and ship the copy at the same path. Keys are dotted lower case, area first.
// A text that contains an apostrophe goes in double quotes, as settings.title does.
//
// Two keys for a word that changes with a count: ".one" for exactly one, ".many" for every other number.
// Tier names (tier.1 to tier.6) also stand in for the names the plugin sends; a deed's label can be given as
// deed.<its editor ID> (for example deed.ROM_LocationsDiscovered); without that line the plugin's own label shows.
// A key missing here shows as [key] and is logged once to the page console.
//
// t(key, values): the text with its {places} filled from values (as text; the page escapes it where it draws it).
// or(key, fallback): the table's text, or fallback (the plugin's own words) when the table has no line for the key.
// Dual-use: Node requires it for the tests.
(function () {
    'use strict';
    const table = {
        'brand.title': 'Romantasy',

        'common.companion': 'Companion',
        'common.newbond': 'New bond',
        'common.deepens': 'Deepens the bond',
        'common.wounds': 'Wounds the bond',
        'common.favorite': 'Favorite',
        'common.peeve': 'Pet peeve',
        'common.cancel': 'Cancel',
        'common.back': 'Back',

        'tier.1': 'Stranger',
        'tier.2': 'Acquaintance',
        'tier.3': 'Friend',
        'tier.4': 'Confidant',
        'tier.5': 'Lover',
        'tier.6': 'Spouse',

        'roster.company': 'Your Company',
        'roster.count.one': '{count} bond',
        'roster.count.many': '{count} bonds',
        'roster.following.one': '· {count} following',
        'roster.following.many': '· {count} following',
        'roster.find': 'Find a companion',
        'roster.find.clear': 'Clear',
        'roster.order.label': 'Order',
        'roster.order.closest': 'Closest',
        'roster.order.name': 'Name',
        'roster.order.recent': 'Recent',
        'roster.group.following': 'Following',
        'roster.group.away': 'Away',
        'roster.row.away': 'Away',
        'roster.nobody': 'No one by that name.',
        'roster.settings': 'Settings',
        'roster.health': 'Health: {status}',
        'roster.refresh': 'Refresh',
        'roster.close': 'Close',

        'dossier.following': 'Following',
        'dossier.away': 'Away',
        'dossier.edit': 'Edit personality',
        'dossier.reset': 'Reset bond',
        'dossier.remove': 'Remove bond',
        'dossier.bond': 'bond',
        'dossier.bond.of': '/ {points} bond',
        'dossier.complete': 'Bond complete',
        'dossier.until': '{points} until {tier}',
        'dossier.gate': '{tier} needs {needed} days together ({sofar} so far)',

        'mood.warm': 'Warm',
        'mood.warm.title': 'Warm: recent days went well.',
        'mood.settled': 'Settled',
        'mood.settled.title': 'Settled: nothing recent weighs on the bond.',
        'mood.strained': 'Strained',
        'mood.strained.title': 'Strained: recent days hurt the bond.',

        'values.title': 'What they value',
        'values.none.likes': 'No likes recorded.',
        'values.none.dislikes': 'No dislikes recorded.',

        'recent.title': 'Recent changes',
        'recent.none': 'No recent changes.',
        'recent.journal': 'Journal',

        'persona.title': 'Personality',
        'persona.configured': 'Configured personality',
        'persona.configured.line': 'Preferences loaded from {file}.',
        'persona.player': 'Player-created personality',
        'persona.player.line': 'This ledger page belongs to you. Its likes and dislikes may be rewritten.',
        'persona.author': 'Author-defined personality',
        'persona.author.line': 'Sealed by the follower author. Romantasy will not alter or remove it.',
        'persona.author.line.plugin': 'Sealed by the follower author in {plugin}. Romantasy will not alter or remove it.',
        'persona.managed': 'Managed personality',
        'persona.managed.line': 'Supplied by another compatible mod at runtime. Romantasy treats it as read-only.',
        'persona.plugin': 'Source plugin',
        'persona.file': 'Profile file',

        'miles.title': 'Milestones',
        'miles.unlocks': '{tier} unlocks:',

        'empty.away': 'Every bond is away. Turn on "Show away companions" in settings to see them.',
        'empty.none': 'No bonds yet. Choose a companion to begin.',
        'empty.settings': 'Settings',

        'settings.eyebrow': 'Sealed Page',
        'settings.title': "The scribe's quill",
        'settings.favorites': 'Open with Favorites Menu',
        'settings.favorites.detail': 'Off: open with Left Ctrl + R. On: cast the Romantasy power from your Favorites menu; the hotkey is disabled.',
        'settings.gain': '"Romance Deepened" popups',
        'settings.gain.detail': 'Show the gold modal when a bond crosses a tier upward.',
        'settings.loss': '"Romance Wounded" popups',
        'settings.loss.detail': 'Show the rose modal when a bond slips below a tier.',
        'settings.away': 'Show away companions',
        'settings.away.detail': 'Keep bonds in the ledger even when not following.',
        'settings.whispers': 'Whispers',
        'settings.whispers.detail': 'Small notes on the HUD when a companion reacts. They never pause the game.',
        'settings.contrast': 'Stronger contrast',
        'settings.contrast.detail': 'Brighter text, solid panels and thicker outlines.',
        'settings.motion': 'Reduce motion',
        'settings.motion.detail': 'Stops the drifting backdrop and the larger animations.',
        'settings.log': 'Detailed log',
        'settings.log.detail': 'Writes every point change to Romantasy.log. For mod authors and bug reports.',
        'settings.dev': 'Developer tools',
        'settings.dev.detail': 'Reveal testing utilities. Enabling prompts for a password.',
        'settings.skyrimnet.bridge': 'Tell SkyrimNet about bonds',
        'settings.skyrimnet.bridge.detail': "Adds each companion's bond to what SkyrimNet knows and reports tier changes to it.",
        'settings.skyrimnet.conversation': 'Conversations can move the bond',
        'settings.skyrimnet.conversation.detail': "Lets SkyrimNet's AI nudge a bond by a few points for a kind or a harsh word. Capped each day.",
        'settings.ostim.moments': 'Shared moments',
        'settings.ostim.moments.detail': 'Time alone with a companion in an OStim scene adds a little to the bond, once a day.',
        'settings.ostim.hold': 'Quiet during scenes',
        'settings.ostim.hold.detail': 'Tier changes and whispers wait until an OStim scene is over.',
        'settings.moment': 'Tier changes arrive',
        'settings.moment.calm': 'At a calm moment',
        'settings.moment.immediate': 'At once',
        'settings.moment.detail': 'A calm moment is an inn, a home, a town, or waking from sleep. A change never waits longer than a day.',
        'settings.pace': 'Relationship pace',
        'settings.pace.slow': 'Slow burn',
        'settings.pace.standard': 'Standard',
        'settings.pace.fast': 'Fast',
        'settings.pace.detail': 'Fast doubles the points from deeds. Slow burn halves them and asks for days together before each higher tier. Losses are never changed.',
        'settings.corner': 'Whisper corner',
        'settings.corner.topleft': 'Top left',
        'settings.corner.topright': 'Top right',
        'settings.corner.bottomleft': 'Bottom left',
        'settings.corner.bottomright': 'Bottom right',
        'settings.size': 'Ledger size',
        'settings.size.standard': 'Standard',
        'settings.size.large': 'Large',
        'settings.size.largest': 'Largest',
        'settings.size.detail': 'Makes everything in the ledger bigger.',

        'dev.title': 'Developer tools',
        'dev.note': 'Testing utilities. Point adjustments and simulated deeds affect only actively following companions.',
        'dev.bond.up': 'Bond +500',
        'dev.bond.down': 'Bond −500',
        'dev.sidequest': 'Side quest +1',
        'dev.murder': 'Murder +1',
        'dev.refresh': 'Refresh',

        'health.title': 'Health',
        'health.ok': 'Everything is in order.',
        'health.word.error': 'Problem',
        'health.word.warn': 'Check',
        'health.meta.plugin': 'Romantasy',
        'health.meta.plugin.version': 'Romantasy {version}',
        'health.meta.host': 'Magelight UI {version}',
        'health.meta.bonds.one': '{count} bond',
        'health.meta.bonds.many': '{count} bonds',

        'enroll.create.eyebrow': 'New bond',
        'enroll.create.title': 'Enter a new bond',
        'enroll.create.copy': 'Choose a companion Romantasy does not know yet, then stamp the deeds that deepen or wound the bond.',
        'enroll.create.commit': 'Begin bond',
        'enroll.edit.eyebrow': 'Player-created personality',
        'enroll.edit.title': 'Rewrite the page',
        'enroll.edit.copy': 'Only pages written by you can be rewritten. Personalities supplied by follower mods stay protected.',
        'enroll.edit.commit': 'Seal personality',
        'enroll.note': 'Your personalities are shared across saves using this configuration. Each save keeps its own earned bond points.',
        'enroll.sub': 'Every deed starts neutral. Stamp the ones that matter to them.',
        'enroll.hint': 'Nothing changes until the page is sealed.',
        'enroll.companion': 'Companion',
        'enroll.notfound': 'Bond not found',
        'enroll.notfound.copy': 'This companion is no longer in the ledger.',
        'enroll.nocandidates': 'No companions are available to add right now. Recruit a follower, then try again.',
        'enroll.nodeeds': 'No deeds are available.',
        'enroll.nomatch': 'No deeds match "{query}".',
        'enroll.deeds': 'Likes & dislikes',
        'enroll.search': 'Search deeds',
        'enroll.category.other': 'Other',
        'enroll.neutral': 'Neutral',
        'enroll.cap.favorites': 'Three favorites at most.',
        'enroll.cap.peeves': 'Three pet peeves at most.',
        'enroll.chips.none': 'Nothing stamped yet.',
        'enroll.deeds.one': '{count} deed',
        'enroll.deeds.many': '{count} deeds',

        'modal.eyebrow': 'Player-created bond',
        'modal.reset.title': 'Reset this bond?',
        'modal.reset.button': 'Reset bond',
        'modal.reset.body': "The personality remains. This companion's points and recent history reset in the current save only.",
        'modal.remove.title': 'Remove this bond?',
        'modal.remove.button': 'Remove bond',
        'modal.remove.body': 'This removes the bond from all saves using this configuration. Personalities supplied by follower mods remain protected.',
        'modal.password.eyebrow': 'Restricted',
        'modal.password.title': 'DEVELOPER TOOLS',
        'modal.password.note': 'These utilities can rewrite every bond. Enter the password to unlock them.',
        'modal.password.field': 'Password',
        'modal.password.error': 'The seal does not recognize that password.',
        'modal.password.unlock': 'Unlock',

        'popup.gain': 'Romance Deepened',
        'popup.loss': 'Romance Wounded',
        'popup.unlocked': 'Unlocked',
        'popup.points': '{points} points',
        'popup.next': 'Next: {tier}',
        'popup.continue': 'Continue',

        'journal.title': 'Journal',
        'journal.opening': 'Opening the journal…',
        'journal.failed': 'The journal could not be opened.',
        'journal.empty': 'Nothing is written here yet.',
        'journal.since': 'Since Day {day} · {together}',
        'journal.together.one': '{days} day together',
        'journal.together.many': '{days} days together',
        'journal.milestones': 'Milestones',
        'journal.milestone': 'Day {day} · {what}',
        'journal.began': 'Journal began',
        'journal.reached': 'Reached {tier}',
        'journal.fell': 'Fell to {tier}',
        'journal.shared': 'A first moment together',
        'journal.chart': 'The last sixty days',
        'journal.chart.tip': 'Day {day}: +{gained}, −{lost}',
        'journal.chart.totals': 'Gained {gained} · Lost {lost}',
        'journal.day': 'Day {day}',
        'journal.moments': 'Biggest moments',
        'journal.moments.gains': 'Deepened most',
        'journal.moments.losses': 'Wounded most',
        'journal.moments.none': 'None yet.',
        'journal.history': 'History',

        'legend.select': 'Select',
        'legend.open': 'Open',
        'legend.close': 'Close',
        'legend.back': 'Back',
        'legend.scroll': 'Scroll',
        'legend.stamp': 'Stamp',
        'legend.cancel': 'Cancel',
        'legend.confirm': 'Confirm',
        'legend.continue': 'Continue',
        'legend.key.enter': 'Enter',
        'legend.key.esc': 'Esc',
        'legend.pad.up': 'D-pad Up',
        'legend.pad.down': 'D-pad Down',
        'legend.pad.updown': 'D-pad ↑↓'
    };

    const has = key => typeof key === 'string' && Object.prototype.hasOwnProperty.call(table, key);
    const logged = {};

    // Each {place} with a value of that name in values; a place with no value stays as written. A table value that
    // is not a string (a hand edit gone wrong) is filled in its string form, never thrown on.
    function fill(text, values) {
        if (!values || typeof values !== 'object') return text;
        return String(text).replace(/\{(\w+)\}/g, (place, name) => Object.prototype.hasOwnProperty.call(values, name) ? String(values[name]) : place);
    }

    function t(key, values) {
        if (has(key)) return fill(table[key], values);
        const shown = '[' + String(key) + ']';
        if (!logged[shown]) {
            logged[shown] = true;
            if (typeof console === 'object' && console && typeof console.warn === 'function') console.warn('Romantasy: strings.js has no text for ' + shown);
        }
        return shown;
    }

    const or = (key, fallback) => has(key) ? table[key] : fallback;

    const api = { t, or, table };
    if (typeof module === 'object' && module.exports) { module.exports = api; return; }
    window.RM = window.RM || {};
    window.RM.strings = api;
})();
