import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, readdirSync, readFileSync as readFileRaw } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

// A checkout may write CRLF or LF; the checks read text as LF. Reads without 'utf8' stay byte-exact.
const readFileSync = (path, encoding) =>
    encoding === 'utf8' ? readFileRaw(path, 'utf8').replace(/\r\n/g, '\n') : readFileRaw(path, encoding);

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const commonLibDir = join(root, 'lib', 'commonlibsse-ng');
const expectedCommit = '7a60f4de794095d7b0f8928d1b930a52e9a7da83';

const git = (...args) => execFileSync('git', args, { encoding: 'utf8' }).trim();
const gitmodules = readFileSync(join(root, '.gitmodules'), 'utf8');
const project = readFileSync(join(root, 'xmake.lua'), 'utf8');
const commonLibBuild = readFileSync(join(commonLibDir, 'xmake.lua'), 'utf8');
const commonLibInterfaces = readFileSync(
    join(commonLibDir, 'include', 'SKSE', 'Interfaces.h'),
    'utf8'
);
const pluginTemplate = readFileSync(
    join(commonLibDir, 'res', 'commonlibsse-ng-plugin.cpp.in'),
    'utf8'
);

assert.match(
    gitmodules,
    /url\s*=\s*https:\/\/github\.com\/alandtse\/CommonLibSSE-NG\.git/,
    'Romantasy must use the maintained CommonLibSSE-NG repository'
);
assert.equal(
    git('-C', commonLibDir, 'rev-parse', 'HEAD'),
    expectedCommit,
    'Romantasy must pin the reviewed CommonLibSSE-NG 7.2.0 release commit'
);

for (const runtime of ['skyrim_se', 'skyrim_ae', 'skyrim_vr']) {
    const optionBlock = new RegExp(
        `option\\("${runtime}"[\\s\\S]*?set_default\\(true\\)[\\s\\S]*?end\\)`,
        'm'
    );
    assert.match(commonLibBuild, optionBlock, `${runtime} must remain enabled by default`);
}

assert.match(project, /address_library\s*=\s*true/);
assert.match(project, /struct_dependent\s*=\s*false/);
assert.match(
    commonLibInterfaces,
    /versionIndependenceEx\s*=\s*kVersionIndependentEx_AddressLibraryV5/,
    'CommonLib must emit Address Library v5 compatibility metadata'
);
assert.match(
    pluginTemplate,
    /StructCompatibility\s*=\s*\$\{COMMONLIBSSE_NG_OPTION_STRUCT_COMPATIBILITY\}/
);
assert.match(
    pluginTemplate,
    /RuntimeCompatibility\s*=\s*\$\{COMMONLIBSSE_NG_OPTION_RUNTIME_COMPATIBILITY\}/
);

// The manager queues game tasks only after its lock is released (OutboxLock::Defer);
// the two direct AddTask calls left are the OutboxLock dispatcher and the load HUD line.
const romanceManager = readFileSync(join(root, 'src', 'romance', 'RomanceManager.cpp'), 'utf8');
assert.equal(
    romanceManager.split('AddTask(').length - 1,
    2,
    'RomanceManager.cpp must call AddTask only from the OutboxLock dispatcher and LoadCallback'
);
assert.ok(
    !romanceManager.includes('std::scoped_lock'),
    'RomanceManager.cpp must take its lock through OutboxLock::Scope'
);

// Favorites, pet peeves and tier unlock lines reach the page; the manager validates
// weights through the pure profile check.
for (const key of ['"favorites"', '"petPeeves"', '"unlocks"']) {
    assert.ok(romanceManager.includes(key), `RomanceManager.cpp must emit the ${key} key`);
}
// The points formula and the Papyrus direction go through the tested pure functions.
assert.ok(
    romanceManager.split('PreferencePoints(').length - 1 >= 2,
    'RomanceManager.cpp must compute preference points with PreferencePoints at both sites'
);
assert.ok(
    romanceManager.includes('PapyrusDirection('),
    'RomanceManager.cpp must answer GetPreference through PapyrusDirection'
);
// The unlock lines are emitted by the state-row builder and by the level-change builder.
const stateBuilder = romanceManager.match(/RomanceManager::BuildStateJson\([\s\S]*?\r?\n\}\r?\n/);
const levelBuilder = romanceManager.match(/RomanceManager::BuildLevelChangeJson\([\s\S]*?\r?\n\}\r?\n/);
assert.ok(stateBuilder && stateBuilder[0].includes('"unlocks"'), 'BuildStateJson must emit "unlocks"');
assert.ok(levelBuilder && levelBuilder[0].includes('"unlocks"'), 'BuildLevelChangeJson must emit "unlocks"');
const validate = romanceManager.match(/RomanceManager::ValidatePreferenceProfile\([\s\S]*?\r?\n\}\r?\n/);
assert.ok(validate, 'RomanceManager::ValidatePreferenceProfile must exist');
assert.ok(
    validate[0].includes('CheckPreferenceWeights('),
    'ValidatePreferenceProfile must call CheckPreferenceWeights'
);

// Mood and the three new events. The new events go through the deferred bark sender;
// the gain event is still sent only for a gain.
for (const name of ['Romantasy_OnLevelLost', 'Romantasy_OnTimeApart', 'Romantasy_OnReunion']) {
    assert.ok(
        romanceManager.includes(`SendBarkEvent(_lock, "${name}"`),
        `RomanceManager.cpp must send ${name} through SendBarkEvent(_lock, ...)`
    );
}
const levelChangedSends = romanceManager.split('"Romantasy_OnLevelChanged"').length - 1;
assert.equal(levelChangedSends, 1, 'Romantasy_OnLevelChanged must be sent from exactly one place');
assert.match(
    romanceManager,
    /if \(newLevel > oldLevel\) \{[^{}]*\{\s*SendBarkEvent\(_lock, "Romantasy_OnLevelChanged"/,
    'Romantasy_OnLevelChanged must be sent only where newLevel > oldLevel'
);
const papyrusScript = readFileSync(join(root, 'Source', 'Scripts', 'Romantasy.psc'), 'utf8');
assert.match(papyrusScript, /^Int Function GetMood\(Actor akFollower\)[^\r\n]*Native/m, 'Romantasy.psc must declare GetMood');
assert.match(papyrusScript, /^String Function GetMoodName\(Actor akFollower\)[^\r\n]*Native/m, 'Romantasy.psc must declare GetMoodName');
const papyrusBridge = readFileSync(join(root, 'src', 'papyrus', 'PapyrusBridge.cpp'), 'utf8');
for (const name of ['GetMood', 'GetMoodName']) {
    assert.ok(
        papyrusBridge.includes(`RegisterFunction("${name}", kScriptName, ${name})`),
        `PapyrusBridge.cpp must register ${name}`
    );
}
assert.ok(stateBuilder[0].includes('"mood"'), 'BuildStateJson must emit "mood"');
// Evaluate has no answer for Mood; the dialogue hook must answer it before dereferencing.
const conditionHook = readFileSync(join(root, 'src', 'conditions', 'RomanceConditionHook.cpp'), 'utf8');
const moodBranch = conditionHook.indexOf('Query::Mood');
assert.ok(
    moodBranch >= 0 && moodBranch < conditionHook.indexOf('*romantasy::conditions::Evaluate('),
    'RomanceConditionHook.cpp must answer Query::Mood through QueryMood before Evaluate'
);
assert.ok(conditionHook.includes('QueryMood('), 'RomanceConditionHook.cpp must call QueryMood');

// A tier lost is sent in the else of the gain test.
assert.match(
    romanceManager,
    /if \(newLevel > oldLevel\) \{[\s\S]*?"Romantasy_OnLevelChanged"[\s\S]*?\} else \{[^}]*?\{\s*const std::string levelName[^}]*SendBarkEvent\(_lock, "Romantasy_OnLevelLost"/,
    'Romantasy_OnLevelLost must be sent in the else of the newLevel > oldLevel test'
);
// Company is observed where the design says, and never inside the cosave load callback,
// where the calendar may not be restored yet.
const body = (name) => {
    const found = romanceManager.match(new RegExp(`RomanceManager::${name}\\([\\s\\S]*?\\r?\\n\\}\\r?\\n`));
    assert.ok(found, `RomanceManager::${name} must exist`);
    return found[0];
};
for (const name of ['OnDialogueClosed', 'OnGameLoaded', 'BuildStateJson']) {
    assert.ok(body(name).includes('ObserveCompany();'), `RomanceManager::${name} must call ObserveCompany()`);
}
const statSink = romanceManager.match(/const RE::TESTrackedStatsEvent\* event,[\s\S]*?\r?\n\}\r?\n/);
assert.ok(statSink && statSink[0].includes('ObserveCompany();'), 'the statistic event path must call ObserveCompany()');
assert.ok(!body('Load').includes('ObserveCompany'), 'RomanceManager::Load must not call ObserveCompany');
assert.ok(!body('Load').includes('GameDay('), 'RomanceManager::Load must not read the game day');
const saveBody = body('Save');
assert.ok(
    saveBody.indexOf('SavePoints(') >= 0 && saveBody.indexOf('SavePoints(') < saveBody.indexOf('SaveMoods('),
    'Save must write the moods record after the points record'
);
// The save callback looks at who is following before the moods record is written, under the
// same lock scope, so a change made just before a save is in the file.
const saveCallback = body('SaveCallback');
const scopeAt = saveCallback.indexOf('OutboxLock::Scope');
const observeAt = saveCallback.indexOf('ObserveCompany();');
const writeAt = saveCallback.indexOf('.Save(');
assert.ok(
    scopeAt >= 0 && observeAt > scopeAt && writeAt > observeAt,
    'SaveCallback must open the lock scope, call ObserveCompany() and then Save()'
);
// A look that fails must not cost the save: Save() runs after the try block, not inside it.
assert.match(
    saveCallback,
    /try \{\s*manager\.ObserveCompany\(\);\s*\} catch \([^)]*\) \{[\s\S]*?\}\s*saved = manager\.Save\(/,
    'SaveCallback must guard the look and call Save() after it either way'
);
// A lost tier weighs on mood whether or not level-change notifications were requested.
assert.match(
    romanceManager,
    /if \(newLevel < oldLevel\) \{\s*bond\.mood = romantasy::mood::Nudged\(bond\.mood, romantasy::mood::kTierLostNudge, today\);\s*\}\s*if \(showLevelUp && newLevel != oldLevel\)/,
    'the tier-lost nudge must sit under its own tier-loss test, before the showLevelUp block'
);
assert.equal(romanceManager.split('kTierLostNudge').length - 1, 1, 'the tier-lost nudge must be applied in one place');
assert.ok(saveBody.includes('SaveMoods('), 'Save must write the moods record');
assert.ok(body('SaveMoods').includes('WriteMoods('), 'SaveMoods must write through WriteMoods');
const mainSource = readFileSync(join(root, 'src', 'main.cpp'), 'utf8');
const postLoad = mainSource.match(/case SKSE::MessagingInterface::kPostLoadGame:[\s\S]*?break;/);
assert.ok(postLoad && postLoad[0].includes('OnGameLoaded()'), 'main.cpp must call OnGameLoaded in the kPostLoadGame block');
assert.equal(romanceManager.split('AddTask(').length - 1, 2, 'RomanceManager.cpp must still call AddTask exactly twice');

// The two pacing settings are stored as key strings, never as the enum's number, and a bad value keeps its default.
const settingsSource = readFileSync(join(root, 'src', 'settings', 'Settings.cpp'), 'utf8');
assert.ok(settingsSource.includes('pacing::Key(_pace)') && settingsSource.includes('pacing::Key(_tierMoment)'),
    'Settings::Save must write pace and tierMoment through pacing::Key');
assert.ok(settingsSource.includes('pacing::ParsePace') && settingsSource.includes('pacing::ParseTierMoment'),
    'Settings::Load must read pace and tierMoment through the Pacing.h parsers');
assert.ok(/ReadKey\(\s*j,\s*"pace"/.test(settingsSource) && /ReadKey\(\s*j,\s*"tierMoment"/.test(settingsSource),
    'Settings::Load must read the pace and tierMoment keys');
const readKeyBody = settingsSource.match(/ReadKey\([\s\S]*?\n    \}/);
assert.ok(readKeyBody && readKeyBody[0].includes('is_string()'), 'a non-string pace or tierMoment must be ignored');
assert.ok(!/\.value\(\)/.test(readKeyBody[0]), 'an unknown pace string must not throw');
const stateSource = romanceManager.slice(romanceManager.indexOf('RomanceManager::BuildStateJson'));
assert.ok(stateSource.includes('{ "pace", romantasy::pacing::Key(Settings::GetSingleton().Pace()) }') &&
    stateSource.includes('{ "tierMoment", romantasy::pacing::Key(Settings::GetSingleton().TierMoment()) }'),
    'BuildStateJson must send pace and tierMoment as key strings');

// The two whisper settings: the corner is stored as its key string through the Whispers.h parser, like pace;
// whispers is a JSON boolean read only when it is one, so a missing or wrong-typed value keeps the default
// and never throws (a j.value() of the wrong type would throw and fail the load of every other key).
assert.ok(/ReadKey\(\s*j,\s*"whisperCorner",\s*_whisperCorner,\s*romantasy::ui::whispers::ParseWhisperCorner\s*\)/.test(settingsSource),
    'Settings::Load must read whisperCorner through ReadKey and ParseWhisperCorner');
assert.ok(settingsSource.includes('j["whisperCorner"]     = std::string(romantasy::ui::whispers::Key(_whisperCorner.load()));'),
    'Settings::Save must write whisperCorner as its key string');
assert.ok(settingsSource.includes('j["whispers"]          = _whispers.load();'), 'Settings::Save must write whispers as a JSON boolean');
const whispersRead = settingsSource.match(/const auto whispers = j\.find\("whispers"\);[\s\S]*?\n        \}/);
assert.ok(whispersRead, 'Settings::Load must look the whispers key up with find');
assert.ok(/is_boolean\(\)[\s\S]*get<bool>\(\)/.test(whispersRead[0]), 'Settings::Load must test whispers is a boolean before reading it');
assert.ok(!/j\.value\(\s*"whispers"/.test(settingsSource), 'Settings::Load must not read whispers with j.value');
// The comfort settings: two keyed strings through the PageRequests parsers, two switches read only when boolean.
assert.ok(/ReadKey\(\s*j,\s*"rosterOrder",\s*_rosterOrder,\s*romantasy::ui::ParseRosterOrder\s*\)/.test(settingsSource), 'Settings::Load must read rosterOrder through ReadKey and ParseRosterOrder');
assert.ok(/ReadKey\(\s*j,\s*"ledgerSize",\s*_ledgerSize,\s*romantasy::ui::ParseLedgerSize\s*\)/.test(settingsSource), 'Settings::Load must read ledgerSize through ReadKey and ParseLedgerSize');
for (const key of ['highContrast', 'reduceMotion']) {
    assert.ok(settingsSource.includes(`ReadSwitch(j, "${key}", _${key});`), `Settings::Load must read ${key} through ReadSwitch`);
    assert.ok(!new RegExp(`j\\.value\\(\\s*"${key}"`).test(settingsSource), `Settings::Load must not read ${key} with j.value`);
    assert.ok(settingsSource.includes(`j["${key}"]`) && stateSource.includes(`{ "${key}", Settings::GetSingleton().${key[0].toUpperCase() + key.slice(1)}() }`), `${key} must be saved and sent as a boolean`);
}
// The stored view must always come from the parser (static text), never from a caller's string.
assert.ok(/Settings::SetRosterOrder\([^)]*\)\s*\{[^}]*ParseRosterOrder\(/.test(settingsSource), 'SetRosterOrder must canonicalise through ParseRosterOrder');
assert.ok(/Settings::SetLedgerSize\([^)]*\)\s*\{[^}]*ParseLedgerSize\(/.test(settingsSource), 'SetLedgerSize must canonicalise through ParseLedgerSize');
for (const file of ['Settings.cpp', 'Settings.h']) {
    const text = readFileSync(join(root, 'src', 'settings', file), 'utf8');
    assert.ok(!/_(rosterOrder|ledgerSize)\s*=\s*value/.test(text), `${file} must not store a borrowed view in the roster order or ledger size`);
}
assert.ok(stateSource.includes('{ "rosterOrder", std::string(Settings::GetSingleton().RosterOrder()) }') &&
    stateSource.includes('{ "ledgerSize", std::string(Settings::GetSingleton().LedgerSize()) }'), 'BuildStateJson must send rosterOrder and ledgerSize as key strings');
assert.ok(stateSource.includes('{ "whispers", Settings::GetSingleton().Whispers() }') &&
    stateSource.includes('{ "whisperCorner", romantasy::ui::whispers::Key(Settings::GetSingleton().WhisperCorner()) }'),
    'BuildStateJson must send whispers as a boolean and whisperCorner as its key string');

// Pace: only the statistic path multiplies a gain; follower scripts' amounts are never paced.
assert.ok(body('ApplyStatDelta').includes('pacing::ApplyPace('), 'ApplyStatDelta must pace gains through pacing::ApplyPace');
assert.equal(romanceManager.split('ApplyPace(').length - 1, 1, 'ApplyPace must be called from the statistic path only');
for (const name of ['ModifyPoints', 'ApplyPreference', 'AddPoints']) {
    assert.ok(!body(name).includes('ApplyPace'), `RomanceManager::${name} must not pace the amount`);
}
// Gate: Slow burn's days-together gate is applied where points are added.
assert.ok(body('AddPoints').includes('pacing::GateGain('), 'AddPoints must gate gains through pacing::GateGain');
// Waiting tier changes merge, wait for a calm moment and are announced from one place.
assert.ok(romanceManager.includes('pacing::Merge('), 'RomanceManager.cpp must merge waiting tier changes with pacing::Merge');
assert.ok(body('ReleaseTierChanges').includes('pacing::DecideRelease('),
    'ReleaseTierChanges must decide every waiting change through pacing::DecideRelease');
assert.equal(romanceManager.split('"Romantasy_OnLevelLost"').length - 1, 1, 'Romantasy_OnLevelLost must be sent from exactly one place');
// Where a calm moment is looked for.
for (const name of ['OnDialogueClosed']) {
    assert.ok(body(name).includes('ReleaseTierChanges('), `RomanceManager::${name} must look for a calm moment`);
}
assert.ok(statSink[0].includes('ReleaseTierChanges('), 'the statistic event path must look for a calm moment');
const combatSink = romanceManager.match(/const RE::TESCombatEvent\* event,[\s\S]*?\r?\n\}\r?\n/);
assert.ok(combatSink && combatSink[0].includes('ReleaseTierChanges('), 'the end of combat must look for a calm moment');
assert.ok(combatSink[0].indexOf('event->actor.get() != player') < combatSink[0].indexOf('OutboxLock::Scope'),
    'combat between other actors must not take the manager lock');
// A change made by a script or a developer tool looks for a calm moment in the same scope.
for (const name of ['ModifyPoints', 'ApplyPreference', 'DebugAddPoints', 'DebugApplyStat', 'DebugDeed', 'DebugSetPoints']) {
    assert.ok(body(name).includes('ReleaseTierChanges(false)'), `RomanceManager::${name} must look for a calm moment after a change`);
}
// A held deed is still felt: the statistic path reports through the pure award rule.
assert.ok(body('ApplyStatDelta').includes('pacing::AwardForStat('), 'ApplyStatDelta must report through pacing::AwardForStat');
// Pace and tier moment are read on the Papyrus thread and written on the game thread.
const settingsHeader = readFileSync(join(root, 'src', 'settings', 'Settings.h'), 'utf8');
for (const field of ['std::atomic<romantasy::pacing::Pace> _pace', 'std::atomic<romantasy::pacing::TierMoment> _tierMoment', 'std::atomic_bool _authorMode',
    'std::atomic_bool _whispers = true', 'std::atomic<romantasy::ui::whispers::WhisperCorner> _whisperCorner = romantasy::ui::whispers::WhisperCorner::TopRight']) {
    assert.ok(settingsHeader.includes(field), `Settings.h must declare ${field}`);
}
for (const type of ['TESActorLocationChangeEvent', 'TESSleepStopEvent', 'TESWaitStopEvent']) {
    const sink = romanceManager.match(new RegExp(`const RE::${type}\\* event,[\\s\\S]*?\\r?\\n\\}\\r?\\n`));
    assert.ok(sink && sink[0].includes('ReleaseTierChanges('), `the ${type} sink must look for a calm moment`);
    assert.ok(sink[0].includes('_viewDirty = true;'), `the ${type} sink must set _viewDirty`);
    assert.ok(romanceManager.includes(`AddEventSink<RE::${type}>(this)`), `the ${type} sink must be registered`);
}
for (const type of ['TESSleepStopEvent', 'TESWaitStopEvent']) {
    const sink = romanceManager.match(new RegExp(`const RE::${type}\\* event,[\\s\\S]*?\\r?\\n\\}\\r?\\n`));
    assert.ok(sink[0].includes('ReleaseTierChanges(!event->interrupted'), `the ${type} sink must count only an uninterrupted rest`);
}
// The waiting changes are saved after every other record and restored like points.
assert.ok(romanceManager.includes("RecordType('R', 'o', 'P', 'n')"), 'the waiting changes must use the RoPn record');
assert.ok(
    saveBody.indexOf('SavePlayerBonds(') >= 0 && saveBody.indexOf('SavePending(') > saveBody.indexOf('SavePlayerBonds('),
    'Save must write the waiting-change record after the other records'
);
assert.match(saveBody, /return [^;]*&& pending\b[^;]*;/, 'a failed waiting-change record must make Save return false');
assert.ok(body('SavePending').includes('WritePending('), 'SavePending must write through WritePending');
assert.ok(body('Load').includes('ReadPending(') && body('Load').includes('DecideRestore('),
    'Load must read the waiting changes through ReadPending and DecideRestore');
for (const name of ['Load', 'Revert']) {
    assert.ok(body(name).includes('_waitingTierChanges.clear();') && body(name).includes('_paceCarry.clear();'),
        `RomanceManager::${name} must clear waiting tier changes and pace carries`);
}
// The bond journal: recorded where points are added, saved as RoJn after every other record,
// restored like points, and answered on its own topic.
assert.ok(body('AddPoints').includes('journal::Record('), 'AddPoints must record the change in the journal');
assert.match(body('AddPoints'),
    /const auto applied = follower\.points - oldPoints;[\s\S]*?journal::Record\(JournalFor\(follower\), reason, applied,/,
    'AddPoints must record the applied change: the new total minus the old total');
assert.ok(body('EnrollPlayerFollower').includes('journal::Begin('), 'a bond created from the page must begin its journal');
assert.ok(romanceManager.includes("RecordType('R', 'o', 'J', 'n')"), 'the journals must use the RoJn record');
assert.ok(
    saveBody.indexOf('SavePending(') >= 0 && saveBody.indexOf('SaveJournals(') > saveBody.indexOf('SavePending('),
    'Save must write the journal record after the waiting-change record'
);
assert.match(saveBody, /return [^;]*&& journals\b/, 'a failed journal record must make Save return false');
assert.ok(body('SaveJournals').includes('WriteJournals('), 'SaveJournals must write through WriteJournals');
assert.ok(body('Load').includes('ReadJournals(') && body('Load').includes('journal::Normalize('),
    'Load must read the journals through ReadJournals and repair each with journal::Normalize');
for (const name of ['Load', 'Revert']) {
    assert.ok(body(name).includes('_journals.clear();') && body(name).includes('_orphanedJournals.clear();'),
        `RomanceManager::${name} must clear the journals`);
}
assert.ok(body('RemovePlayerFollower').includes('_journals.erase('), 'removing a bond must erase its journal');
assert.ok(body('ResetPlayerFollowerPoints').includes('journal::RecordReset('), 'resetting a bond writes the reset in its journal as a fresh start');
assert.ok(!body('ResetPlayerFollowerPoints').includes('journal::Record('), 'a reset is not recorded as a wound');
assert.ok(body('SaveJournals').includes('journal::Normalize('), 'SaveJournals must write each live journal normalised');
const enrollBody = body('EnrollPlayerFollower');
assert.ok(enrollBody.includes('_journals.erase(identityFormID);') && enrollBody.indexOf('_journals.erase(') < enrollBody.indexOf('journal::Begin('),
    'a bond enrolled from the ledger must start a clean journal');
assert.ok(!body('ResetPlayerFollowerPoints').includes('_journals.erase('), 'resetting a bond keeps its journal');
// Shared moments: saved as RoMo after the journal, restored and kept as the journal is, and never part of RoJn.
assert.ok(romanceManager.includes("RecordType('R', 'o', 'M', 'o')"), 'the moments must use the RoMo record');
assert.ok(saveBody.indexOf('SaveMoments(') > saveBody.indexOf('SaveJournals('), 'Save must write the moments record after the journal record');
assert.match(saveBody, /return [^;]*&& moments;/, 'a failed moments record must make Save return false');
assert.ok(body('SaveMoments').includes('WriteMoments(') && body('SaveMoments').includes('_orphanedMoments'),
    'SaveMoments must write through WriteMoments and keep the orphaned moments');
assert.ok(body('Load').includes('ReadMoments(') && body('Load').includes('_orphanedMoments.push_back('),
    'Load must read the moments through ReadMoments and keep those whose plugin is gone');
for (const name of ['Load', 'Revert']) {
    assert.ok(body(name).includes('_moments.clear();') && body(name).includes('_orphanedMoments.clear();'),
        `RomanceManager::${name} must clear the moments`);
}
assert.ok(body('RemovePlayerFollower').includes('_moments.erase(identityFormID);') && body('RemovePlayerFollower').includes('_moments.erase(baseFormID);') &&
    body('RemovePlayerFollower').includes('_moments.erase(SavedPointsKey(entry));'), 'removing a bond must erase its moments');
assert.ok(!body('ResetPlayerFollowerPoints').includes('_moments'), 'resetting a bond keeps its moments');
assert.ok(enrollBody.includes('_moments.erase(identityFormID);') && enrollBody.includes('_moments.erase(baseNpc->GetFormID());'),
    'a bond enrolled from the ledger must start with clean moments');
assert.ok(body('MomentsFor').includes('_moments.erase(legacy);'), 'MomentsFor must move a base-keyed entry to the reference');
assert.ok(body('BuildJournalJson').includes('firstDay'), 'BuildJournalJson must pass the first moment\'s day');
const saveCodecHeader = readFileSync(join(root, 'src', 'romance', 'SaveCodec.h'), 'utf8');
assert.ok(saveCodecHeader.includes('inline constexpr std::uint32_t kJournalsVersion = 1;'), 'the journal record stays version 1');
assert.ok(saveCodecHeader.includes('inline constexpr std::uint32_t kMomentsVersion = 1;'), 'the moments record is version 1');
const journalBuilder = body('BuildJournalJson');
assert.ok(journalBuilder.includes('OutboxLock::Scope') && journalBuilder.includes('_viewDirty = true;') &&
    journalBuilder.includes('JournalJson('), 'BuildJournalJson must take the lock, set _viewDirty and build through JournalJson');
const uiSource = readFileSync(join(root, 'src', 'ui', 'RomantasyUI.cpp'), 'utf8');
assert.ok(uiSource.includes('name == "romantasyJournalOpen"') && uiSource.includes('ParseJournalRequest('),
    'RomantasyUI must handle romantasyJournalOpen through ParseJournalRequest');
assert.ok(uiSource.includes('channels::kJournal') && uiSource.includes('BuildJournalJson('),
    'RomantasyUI must publish BuildJournalJson on the journal topic');

// The page's gate line.
assert.ok(stateBuilder[0].includes('"gate"'), 'BuildStateJson must emit "gate"');
assert.ok(stateBuilder[0].includes('pacing::NextGate('), 'the gate row must come from pacing::NextGate');
// Pacing.h counts tiers itself (500 points each, Spouse = 6); the manager's tiers must agree.
const pacingSource = readFileSync(join(root, 'src', 'romance', 'Pacing.h'), 'utf8');
assert.ok(/kTierSize = 500;/.test(pacingSource) && /kTopLevel = 6;/.test(pacingSource), 'Pacing.h tiers must be 500 points up to level 6');
const levelNumbers = [...body('LevelNumberForPoints').matchAll(/points >= (\d+)\) \{\s*return (\d+);/g)]
    .map((m) => [Number(m[1]), Number(m[2])]);
assert.deepEqual(levelNumbers, [[2500, 6], [2000, 5], [1500, 4], [1000, 3], [500, 2]],
    'LevelNumberForPoints must keep the tiers Pacing.h assumes');

// Author mode: six debug natives, refused unless settings.json says authorMode, and never in the page.
const debugNatives = [
    ['DebugDeed', 'Bool Function DebugDeed(Actor follower, String statistic, Int count = 1) Native Global'],
    ['DebugSetPoints', 'Bool Function DebugSetPoints(Actor follower, Int points) Native Global'],
    ['DebugSetDaysTogether', 'Bool Function DebugSetDaysTogether(Actor follower, Float days) Native Global'],
    ['DebugSetMood', 'Bool Function DebugSetMood(Actor follower, Int score) Native Global'],
    ['DebugAnnounceNow', 'Bool Function DebugAnnounceNow() Native Global'],
    ['DebugDescribe', 'String Function DebugDescribe(Actor follower) Native Global'],
];
for (const [name, declaration] of debugNatives) {
    assert.ok(papyrusScript.split(/\r?\n/).includes(declaration), `Romantasy.psc must declare ${declaration}`);
    assert.ok(papyrusBridge.includes(`RegisterFunction("${name}", kScriptName, ${name})`), `PapyrusBridge.cpp must register ${name}`);
    const native = papyrusBridge.match(new RegExp(`\\n    \\S[^\\n(]* ${name}\\(\\s*RE::StaticFunctionTag\\*[^)]*\\)\\s*\\{\\s*([^\\n]*)`));
    assert.ok(native, `PapyrusBridge.cpp must define the ${name} native`);
    assert.equal(native[1], 'if (!Settings::GetSingleton().AuthorMode()) {', `${name} must check AuthorMode() before anything else`);
}
assert.ok(papyrusBridge.includes('constexpr std::int32_t kApiVersion = 9;'), 'shared moments raise kApiVersion from 8 to 9');
assert.ok(/j\.find\("authorMode"\)|"authorMode"/.test(settingsSource), 'Settings must read and write authorMode');
assert.ok(!/j\.value\("authorMode"/.test(settingsSource), 'a bad authorMode value must not throw and fail the other keys');
const pageSettings = readFileSync(join(root, 'view', 'js', 'settings.js'), 'utf8');
const channelsHeader = readFileSync(join(root, 'src', 'ui', 'magelight', 'Channels.h'), 'utf8');
assert.ok(!pageSettings.includes('authorMode'), 'settings.js must not offer authorMode');
assert.ok(!channelsHeader.match(/kSettingsKeys\{[^}]*\}/)[0].includes('authorMode'), 'authorMode must not be a settings channel key');
// Why-lines are built only inside a DetailedLog() test, so nothing is built when the detailed log is off.
// Every why:: call that builds a line is checked here, none is exempt.
const whyCalls = [...romanceManager.matchAll(/why::(Changed|Unchanged|ScriptUnchanged|Script|Reason|MomentNothing|Moment|NothingText|SceneNothing)\(/g)];
assert.ok(whyCalls.length >= 3, 'RomanceManager.cpp must write why-lines for deeds and script awards');
for (const call of whyCalls) {
    let guarded = false;
    let depth = 0;
    for (let at = call.index; at >= 0 && !guarded; --at) {
        const c = romanceManager[at];
        if (c === '}') depth++;
        else if (c === '{') {
            if (depth > 0) { depth--; continue; }
            const before = romanceManager.slice(Math.max(0, at - 200), at);
            if (/if \([^{};]*DetailedLog\(\)[^{};]*\)\s*$/.test(before)) guarded = true;
        }
    }
    assert.ok(guarded, `why::${call[1]} at offset ${call.index} must be built inside an if (... DetailedLog() ...) block`);
}
for (const name of ['ModifyPoints', 'ApplyPreference']) {
    assert.ok(body(name).includes('why::Script('), `RomanceManager::${name} must write a script-award why-line`);
}
for (const name of ['DebugDeed', 'DebugSetPoints', 'DebugSetDaysTogether', 'DebugSetMood', 'DebugAnnounceNow', 'DebugDescribe']) {
    const entry = body(name);
    assert.ok(entry.includes('OutboxLock::Scope') && entry.includes('_viewDirty = true;'),
        `RomanceManager::${name} must take the lock and set _viewDirty`);
}
assert.ok(body('DebugSetPoints').includes('AddPoints('), 'DebugSetPoints must go through AddPoints');
assert.ok(body('DebugDeed').includes('ApplyStatDelta('), 'DebugDeed must run the statistic path');
// DebugDeed meets the gate as a player would (gated = true); only DebugSetPoints goes around it.
assert.ok(body('DebugDeed').includes('ApplyStatDelta(*rule, count, true, false, true,'), 'DebugDeed must be paced and gated');
assert.ok(/AddPoints\([^;]*, true, false, false\)/.test(body('DebugSetPoints')), 'DebugSetPoints must stay ungated');
// One rule picks a why-line's reason, and one cleaner cleans the author lines.
for (const name of ['ApplyStatDelta', 'ModifyPoints', 'ApplyPreference']) {
    assert.ok(body(name).includes('why::Reason('), `RomanceManager::${name} must choose its why-line through why::Reason`);
}
assert.ok(body('DebugDescribe').includes('why::CleanText(') && body('DebugDeed').includes('why::CleanText(statistic)'),
    'DebugDescribe and the unknown-deed lines must clean text with why::CleanText');
assert.ok(body('DebugAnnounceNow').includes('ReleaseTierChanges(true'), 'DebugAnnounceNow must release as a calm moment (just rested)');

// Whispers: a click-through HUD view that never takes UI mode, raised only through the manager's outbox.
const whisperView = readFileSync(join(root, 'src', 'ui', 'magelight', 'WhisperView.cpp'), 'utf8');
assert.ok(/desc\.clickThrough\s*=\s*true;/.test(whisperView), 'WhisperView must create its view click-through');
assert.ok(/desc\.layer\s*=\s*MAGELIGHT_API::Layer::Hud;/.test(whisperView), 'WhisperView must create its view on the Hud layer');
assert.ok(/desc\.startVisible\s*=\s*false;/.test(whisperView), 'WhisperView must create its view hidden');
assert.ok(!/RequestUIMode|EnterUIMode/.test(whisperView), 'WhisperView must never ask for UI mode');
assert.ok(whisperView.includes('"whisper.html"') || whisperView.includes('L"whisper.html"'), 'WhisperView must load whisper.html');
const romantasyUI = readFileSync(join(root, 'src', 'ui', 'RomantasyUI.cpp'), 'utf8');
const uiBody = (name) => {
    const found = romantasyUI.match(new RegExp(`(?:void|bool) RomantasyUI::${name}\\([\\s\\S]*?\\r?\\n\\}\\r?\\n`));
    assert.ok(found, `RomantasyUI::${name} must exist`);
    return found[0];
};
assert.ok(uiBody('PumpWhispers').includes('.Next('), 'PumpWhispers must call the queue\'s Next');
assert.ok(uiBody('PumpWhispers').includes('GetTickCount64()'), 'PumpWhispers must read GetTickCount64() for the queue');
assert.ok(uiBody('Whisper').includes('PumpWhispers();') && uiBody('Whisper').includes('GetTickCount64()'),
    'Whisper must push with GetTickCount64() and pump');
assert.ok(uiBody('OnWhisperDone').includes('PumpWhispers();') && uiBody('OnWhisperDone').includes('_whispers.Answer('),
    'the romantasyWhisperDone handler must check the seq through the pure Pump and pump');
// Every path on which the ledger or a popup stops blocking pumps, and the ticker is the whispers' beat too.
for (const name of ['CloseAll', 'LeaveEmptyPopup', 'OnHostExited', 'OnHostRefused', 'OnTick']) {
    assert.ok(uiBody(name).includes('PumpWhispers();'), `RomantasyUI::${name} must pump whispers`);
}
assert.ok(uiBody('WhispersChanged').includes('QueueChanged();'), 'a waiting whisper must keep the ticker running');
assert.ok(uiBody('QueueChanged').includes('_whispersPending'), 'the ticker must run while a whisper waits or is on the card');
assert.ok(uiBody('PumpWhispers').includes('NotifyWhisper(') && uiBody('NotifyWhisper').includes('NotificationMarkup(') &&
    !romantasyUI.includes('NotificationText('), 'the fallback must hand the game escaped text');
// A card that stops answering is given up for the session, and a view that dies gives its whisper to the fallback.
assert.ok(uiBody('PumpWhispers').includes('decision.gaveUp') && uiBody('PumpWhispers').includes('view.GiveUp();'),
    'two silent cards in a row must switch whispers to the fallback');
assert.ok(uiBody('OnWhisperCardLost').includes('_whispers.Lost()') && uiBody('OnWhisperCardLost').includes('NotifyWhisper('),
    'the whisper on a card whose view died must go to the fallback');
// Whispers do not cross a load: each carries the generation it was raised in.
assert.ok(uiBody('Close').includes('++_whisperGeneration;'), 'a load or new game must move the whisper generation on');
assert.ok(uiBody('Whisper').includes('_whisperGeneration'), 'a whisper from before the last load must be dropped');
assert.ok(body('HoldOrAnnounceLevelChange').includes('.whisper = !_quietWhispers'), 'a held tier change must keep its origin');
const menuSink = romantasyUI.match(/class RomantasyMenuSink[\s\S]*?\r?\n\};/);
assert.ok(menuSink && menuSink[0].includes('PumpWhispers()'), 'the menu sink must pump whispers when a menu closes');
// The mode carries the three comfort settings, so a popup before the ledger's first state follows them.
const publishModeBody = uiBody('PublishMode');
for (const [key, getter] of [['ledgerSize', 'LedgerSize()'], ['highContrast', 'HighContrast()'], ['reduceMotion', 'ReduceMotion()']]) {
    assert.ok(publishModeBody.includes(`{ "${key}", `) && publishModeBody.includes(getter),
        `the romantasyMode payload must carry ${key} from Settings::${getter}`);
}
const toggleBody = uiBody('Toggle');
assert.ok(toggleBody.indexOf('ClearWhispers();') > toggleBody.indexOf('host.Enter(MagelightHost::Mode::Ledger)'),
    'opening the ledger must clear the whispers');
const tryShowBody = uiBody('TryShowHead');
assert.ok(tryShowBody.indexOf('ClearWhispers();') > tryShowBody.indexOf('host.Enter(MagelightHost::Mode::Popup)'),
    'entering a tier popup must clear the whispers');
assert.ok(uiBody('Close').includes('ClearWhispers();'), 'closing for a load must clear the whispers');
// A tier whisper is raised at the one place a popup is dropped because its direction is switched off.
const suppressed = uiBody('ShowLevelUpModalNow').match(/ShowLossModals\(\)\)[\s\S]*?return;\r?\n    \}/);
assert.ok(suppressed && suppressed[0].includes('WhisperTier(') && suppressed[0].includes('Whispers()'),
    'ShowLevelUpModalNow must raise the tier whisper inside its suppressed-by-settings branch');
// With no usable host the popup cannot show: the tier change is whispered instead. Both branches return before the
// popup is queued, so a tier change never both pops up and whispers.
const modalNow = uiBody('ShowLevelUpModalNow');
const noHost = modalNow.match(/if \(!MagelightHost::GetSingleton\(\)\.HasHost\(\)\) \{[\s\S]*?return;\r?\n    \}/);
assert.ok(noHost && noHost[0].includes('WhisperTier(') && noHost[0].includes('Whispers()'),
    'ShowLevelUpModalNow must whisper a tier change it cannot show for want of a host');
assert.ok(modalNow.indexOf('_queue.Push(') > modalNow.indexOf(noHost[0]) + noHost[0].length - 1 &&
    modalNow.indexOf('_queue.Push(') > modalNow.indexOf(suppressed[0]) + suppressed[0].length - 1,
    'the popup is queued only after both whisper branches have returned');
assert.equal(modalNow.split('WhisperTier(').length - 1, 2, 'a tier whisper is raised from those two branches only');
const whisperCalls = romanceManager.split('RomantasyUI::GetSingleton().Whisper(').length - 1;
assert.equal(whisperCalls, 1, 'RomanceManager.cpp must hand whispers to the UI from one place');
const raise = body('RaiseWhisper');
assert.ok(/_lock\.Defer\(\[[^\]]*\]\(\)[^{]*\{\s*RomantasyUI::GetSingleton\(\)\.Whisper\(/.test(raise),
    'RaiseWhisper must hand the whisper over inside a _lock.Defer lambda');
assert.ok(raise.includes('_quietWhispers') && raise.includes('Settings::GetSingleton().Whispers()'),
    'RaiseWhisper must raise nothing for developer changes or while whispers are off');
for (const name of ['DebugAddPoints', 'DebugApplyStat']) {
    assert.ok(body(name).includes('QuietWhispers quiet(_quietWhispers);'), `${name} must raise no whispers`);
    assert.ok(!body(name).includes('RaiseWhisper('), `${name} must raise no whispers`);
}
assert.ok(!body('DebugDeed').includes('QuietWhispers'), 'DebugDeed raises whispers as a real deed would');
for (const kind of ['Kind::Deed', 'Kind::Reunion', 'Kind::TimeApart']) {
    assert.ok(romanceManager.includes(kind), `RomanceManager.cpp must raise ${kind}`);
}
assert.ok(body('AddPoints').includes('MoodCrossing('), 'AddPoints must raise a mood whisper through MoodCrossing');
const healthGather = readFileSync(join(root, 'src', 'health', 'HealthMonitor.cpp'), 'utf8');
assert.ok(healthGather.includes('facts.whispers =') && healthGather.includes('facts.whisperView ='),
    'the health check must gather the whisper facts');


// The SkyrimNet bridge: found at run time, never loaded or linked by Romantasy; every pointer tested before its call.
const skyrimNetDir = join(root, 'src', 'skyrimnet');
const bridgeSource = readFileSync(join(skyrimNetDir, 'SkyrimNetBridge.cpp'), 'utf8');
const bridgeHeader = readFileSync(join(skyrimNetDir, 'SkyrimNetBridge.h'), 'utf8');
for (const [name, text] of [['SkyrimNetBridge.cpp', bridgeSource], ['SkyrimNetBridge.h', bridgeHeader],
    ['BondLine.cpp', readFileSync(join(skyrimNetDir, 'BondLine.cpp'), 'utf8')], ['BondLine.h', readFileSync(join(skyrimNetDir, 'BondLine.h'), 'utf8')],
    ['ConversationLimits.h', readFileSync(join(skyrimNetDir, 'ConversationLimits.h'), 'utf8')]]) {
    assert.ok(!/LoadLibrary/.test(text), `${name} must never load a library (SkyrimNet is found, not loaded)`);
}
assert.ok(bridgeSource.includes('GetModuleHandleW(L"SkyrimNet.dll")'), 'the bridge must find SkyrimNet with GetModuleHandleW(L"SkyrimNet.dll")');
assert.equal(bridgeSource.split('GetModuleHandle').length - 1, 1, 'the bridge must look for the module once');
assert.ok(!/#pragma\s+comment\s*\(\s*lib/.test(bridgeSource) && !/SkyrimNet\.lib/i.test(project), 'nothing may link against SkyrimNet');
// Each export is resolved into a local or a stored pointer and tested before the first call through it.
const resolved = [...bridgeSource.matchAll(/(?:const auto|auto)\s+(\w+)\s*=\s*reinterpret_cast<\w+>\(\s*GetProcAddress\(/g)].map(m => m[1]);
assert.ok(resolved.length >= 2, 'the bridge must resolve PublicGetVersion and PublicRegisterEvent by name');
for (const name of resolved) {
    const call = bridgeSource.search(new RegExp(String.raw`\b${name}\(`));
    const test = bridgeSource.search(new RegExp(String.raw`(?:!\s*${name}\b|\b${name}\s*\?|\b${name}\s*==\s*nullptr|\b${name}\s*!=\s*nullptr)`));
    assert.ok(call < 0 || (test >= 0 && test < call), `${name} must be tested before it is called`);
}
const sendBody = bridgeSource.match(/void SkyrimNetBridge::SendEvent\([\s\S]*?\r?\n\}\r?\n/);
assert.ok(sendBody, 'SkyrimNetBridge::SendEvent must exist');
const loaded = sendBody[0].match(/const auto (\w+) = g_\w+\.load\(\);/);
assert.ok(loaded && sendBody[0].indexOf(`if (!${loaded[1]}`) > 0 && sendBody[0].indexOf(`if (!${loaded[1]}`) < sendBody[0].indexOf(`${loaded[1]}(`),
    'SendEvent must test the stored PublicRegisterEvent pointer before the call');
assert.ok(sendBody[0].includes('Settings::GetSingleton().SkyrimNetBridge()'), 'SendEvent must send nothing while the bridge is switched off');
assert.ok(/kEventType = "persistent_generic";/.test(bridgeSource) && sendBody[0].includes('(kEventType, '),'events are plain text (persistent_generic)');
// Narration for the follower alone: no originator (one would make it the follower's own speech), the follower as target
// and as the only audience entry. The export answers with the event id, or 0 when it stored nothing: that is said
// once per session and never retried in the old shape.
assert.ok(sendBody[0].includes('const std::uint32_t audience[1]{ follower };') &&
    sendBody[0].includes('send(kEventType, text.c_str(), 0, follower, audience, 1)'),
    'an event has originator 0, the follower as target and an audience of exactly the follower');
assert.equal(sendBody[0].split('send(').length - 1, 1, 'an event is sent once, with no fallback shape');
assert.ok(/if \(id == 0 && !g_nothingStoredLogged\.exchange\(true\)\) \{\s*logger::info\("SkyrimNet stored nothing for a bond event\./.test(sendBody[0]),
    'an event SkyrimNet stored nothing for (id 0) is said once per session');
assert.ok(!bridgeSource.includes('kPlayerFormID'), 'the player is no longer the target of an event');
// SkyrimNet links the static C runtime and Romantasy the DLL one, so no C++ object (std::function, std::string) may
// cross into SkyrimNet: the decorator and the C++ actions are not registered by this build.
assert.ok(!bridgeSource.includes('std::function'), 'no std::function may be handed to SkyrimNet');
assert.ok(!/GetProcAddress\([^)]*"PublicRegister(Decorator|CPPAction|CPPSubCategory|EventCallback)"/.test(bridgeSource),
    'no export that takes a C++ object may be resolved while the C runtimes differ');
// Events: sent from the manager only inside a deferred lambda, so they run on the game thread after the lock.
const skyrimNetSends = romanceManager.split('SkyrimNetBridge::GetSingleton().SendEvent(').length - 1;
assert.ok(skyrimNetSends >= 1, 'RomanceManager.cpp must send SkyrimNet events');
assert.equal((romanceManager.match(/\.Defer\(\s*\[[^\]]*\]\s*\(\)\s*(?:mutable\s*)?\{\s*SkyrimNetBridge::GetSingleton\(\)\.SendEvent\(/g) || []).length,
    skyrimNetSends, 'every SkyrimNet SendEvent must be the body of a Defer( lambda');
for (const text of ['TierGainedText(', 'TierLostText(', 'ReunionText(', 'TimeApartText(']) {
    assert.equal(romanceManager.split(text).length - 1, 1, `RomanceManager.cpp must build ${text.slice(0, -1)} at one place`);
    assert.ok(new RegExp(String.raw`\[&\]\(std::string_view player\) \{ return romantasy::skyrimnet::${text.slice(0, -1)}\([^;]*, player\); \}`).test(romanceManager),
        `${text.slice(0, -1)} must be given the player's name`);
}
// The player's name is read only once SkyrimNet and the switch allow a send, and the bond line carries it too.
const sendHelper = romanceManager.match(/void SendSkyrimNetEvent\([\s\S]*?\r?\n    \}\r?\n/);
assert.ok(sendHelper && sendHelper[0].indexOf('PlayerName()') > sendHelper[0].indexOf('return;'), 'the player name is read after the checks');
assert.ok(/facts\.bond\.playerName = /.test(romanceManager), 'the bond line is given the player\'s name');
const announce = body('AnnounceLevelChange');
assert.ok(announce.indexOf('TierGainedText(') > announce.indexOf('"Romantasy_OnLevelChanged"') &&
    announce.indexOf('TierLostText(') > announce.indexOf('"Romantasy_OnLevelLost"'), 'tier events go where the Papyrus events are sent');
assert.ok(!body('HoldOrAnnounceLevelChange').includes('SendSkyrimNetEvent('), 'a held tier change sends nothing until it is announced');
const company = body('ObserveCompany');
assert.ok(company.indexOf('ReunionText(') > company.indexOf('"Romantasy_OnReunion"') &&
    company.indexOf('TimeApartText(') > company.indexOf('"Romantasy_OnTimeApart"'), 'company events go where the Papyrus events are sent');
// Settings: two switches, on by default, read only when boolean; state JSON carries skyrimNet.
for (const key of ['skyrimNetBridge', 'skyrimNetConversation']) {
    assert.ok(settingsSource.includes(`ReadSwitch(j, "${key}", _${key});`), `Settings::Load must read ${key} through ReadSwitch`);
    assert.ok(settingsSource.includes(`j["${key}"]`), `Settings::Save must write ${key}`);
    assert.ok(new RegExp(`std::atomic_bool _${key} = true;`).test(settingsHeader), `${key} must default to on`);
}
assert.ok(/\{\s*"skyrimNet",\s*\{\s*\{\s*"found",\s*SkyrimNetBridge::GetSingleton\(\)\.Found\(\)\s*\},\s*\{\s*"bridge",\s*Settings::GetSingleton\(\)\.SkyrimNetBridge\(\)\s*\},\s*\{\s*"conversation",\s*Settings::GetSingleton\(\)\.SkyrimNetConversation\(\)\s*\}\s*\}\s*\}/.test(stateSource),
    'BuildStateJson must send skyrimNet { found, bridge, conversation }');
const dataLoaded = mainSource.match(/void OnDataLoaded\(\)[\s\S]*?\r?\n    \}/);
assert.ok(dataLoaded && dataLoaded[0].indexOf('SkyrimNetBridge::GetSingleton().Start();') > dataLoaded[0].indexOf('Settings::GetSingleton().Load();'),
    'the bridge must start at data-loaded, after Settings');
assert.ok(healthGather.includes('facts.skyrimNet =') && healthGather.includes('facts.skyrimNetBridge ='), 'the health check must gather the SkyrimNet facts');
assert.ok(healthGather.includes('facts.skyrimNetBondLineMissing ='), 'the health check must gather whether the bond line failed to register');

// The bond line reaches SkyrimNet through its Papyrus API: a static call dispatched by name, no compile-time reference.
const repoPapyrus = execFileSync('git', ['-C', root, 'ls-files', '*.psc'], { encoding: 'utf8' }).split(/\r?\n/).filter(Boolean);
assert.ok(repoPapyrus.includes('Source/Scripts/Romantasy_SkyrimNet.psc'), 'Romantasy_SkyrimNet.psc must be in the repository');
for (const file of repoPapyrus) {
    assert.ok(!readFileSync(join(root, file), 'utf8').includes('SkyrimNetApi'), `${file} must not reference SkyrimNetApi`);
}
const bondScript = readFileSync(join(root, 'Source', 'Scripts', 'Romantasy_SkyrimNet.psc'), 'utf8');
assert.ok(/String Function GetBondLine\(Actor akActor\) Native Global/i.test(bondScript), 'Romantasy_SkyrimNet must declare the native GetBondLine');
assert.ok(/String Function BondDecorator\(Actor akActor\) Global/i.test(bondScript), 'Romantasy_SkyrimNet must define BondDecorator(Actor) Global');
const dispatch = bridgeSource.match(/void SkyrimNetBridge::RegisterDecorator\(\)[\s\S]*?\r?\n\}\r?\n/);
assert.ok(dispatch, 'SkyrimNetBridge::RegisterDecorator must exist');
for (const name of ['"SkyrimNetApi"', '"RegisterDecorator"', '"romantasy_bond"', '"Romantasy_SkyrimNet"', '"BondDecorator"']) {
    assert.ok(bridgeSource.includes(name), `the bridge must name ${name}`);
}
assert.ok(dispatch[0].includes('DispatchStaticCall('), 'the decorator must be registered through a static Papyrus call');
// Found or too old: the Papyrus registrations need no C export, and the shipped prompt file calls romantasy_bond.
assert.ok(dispatch[0].indexOf('if (!Found() && !TooOld())') >= 0 && dispatch[0].indexOf('if (!Found() && !TooOld())') < dispatch[0].indexOf('DispatchStaticCall('),
    'nothing is dispatched unless SkyrimNet was seen (found or too old)');
assert.ok(!dispatch[0].includes('SkyrimNetBridge()'), 'the decorator is registered even with the bridge switched off');
assert.ok(!/PublicRegisterDecorator\s*\(/.test(bridgeSource) && !bridgeSource.includes('PublicRegisterDecorator'), 'the C++ decorator export must not be used');
for (const kind of ['kNewGame', 'kPostLoadGame']) {
    const block = mainSource.match(new RegExp(`case SKSE::MessagingInterface::${kind}:[\\s\\S]*?break;`));
    assert.ok(block && block[0].includes('SkyrimNetBridge::GetSingleton().RegisterDecorator();'), `${kind} must register the decorator`);
}
const papyrusBridgeSource = readFileSync(join(root, 'src', 'papyrus', 'PapyrusBridge.cpp'), 'utf8');
const bondNative = papyrusBridgeSource.match(/RE::BSFixedString GetBondLine\([\s\S]*?\r?\n    \}\r?\n/);
assert.ok(bondNative && /try\s*\{[\s\S]*\}\s*catch\s*\(\.\.\.\)\s*\{\s*return RE::BSFixedString\(""\);/.test(bondNative[0]),
    'GetBondLine must return through a catch-all');
assert.ok(/RegisterFunction\("GetBondLine",\s*kSkyrimNetScriptName,\s*GetBondLine\)/.test(papyrusBridgeSource) &&
    papyrusBridgeSource.includes('kSkyrimNetScriptName = "Romantasy_SkyrimNet"'), 'GetBondLine must be registered on Romantasy_SkyrimNet');
const bondLineBody = romanceManager.match(/std::string RomanceManager::SkyrimNetBondLine\([\s\S]*?\r?\n\}\r?\n/);
assert.ok(bondLineBody, 'RomanceManager::SkyrimNetBondLine must exist');
assert.ok(!bondLineBody[0].includes('_viewDirty') && !/FindFollower\(|ObserveCompany\(|EnsureFollowersDiscovered\(|Refresh\w*\(/.test(bondLineBody[0]),
    'the bond line read changes nothing');
assert.ok(bondLineBody[0].includes('DecoratorLine('), 'the bond line goes through the pure DecoratorLine');

// Conversation moves a bond: two actions registered through SkyrimNet's Papyrus API, run by Romantasy_SkyrimNet.
const bridgeBody = (name) => {
    const found = bridgeSource.match(new RegExp(`SkyrimNetBridge::${name}\\([\\s\\S]*?\\r?\\n\\}\\r?\\n`));
    assert.ok(found, `SkyrimNetBridge::${name} must exist`);
    return found[0];
};
for (const name of ['"RegisterAction"', '"RomantasyTouched"', '"RomantasyHurt"', '"TouchedEligible"', '"TouchedExecute"',
    '"HurtEligible"', '"HurtExecute"', '"PAPYRUS"']) {
    assert.ok(bridgeSource.includes(name), `the bridge must name ${name}`);
}
assert.ok(bridgeSource.includes('"You are genuinely moved by something kind, thoughtful or loving {{ player.name }} just said or did. Use rarely, only for moments that truly matter to you."'),
    'the touched action must carry its exact description');
assert.ok(bridgeSource.includes('"You are genuinely hurt or angered by something cruel, dismissive or disloyal {{ player.name }} just said or did. Use rarely."'),
    'the hurt action must carry its exact description');
const actions = bridgeBody('RegisterActions');
assert.ok(actions.indexOf('if (!Found() && !TooOld())') >= 0 && actions.indexOf('if (!Found() && !TooOld())') < actions.indexOf('DispatchStaticCall('),
    'no action is dispatched unless SkyrimNet was seen (found or too old)');
assert.ok(!/SkyrimNetBridge\(\)|SkyrimNetConversation\(\)/.test(actions), 'the actions are registered whatever the settings say');
// All twelve arguments: a VM dispatch fills no Papyrus defaults.
// In order: name, description, script, eligibility, script, execution, "", category, priority, "", "", "".
const actionArgs = actions.match(new RegExp(String.raw`RE::MakeFunctionArguments\(\s*` + [
    'RE::BSFixedString\\(action\\.name\\)', 'RE::BSFixedString\\(action\\.description\\)',
    'RE::BSFixedString\\(kDecoratorScript\\)', 'RE::BSFixedString\\(action\\.eligibility\\)',
    'RE::BSFixedString\\(kDecoratorScript\\)', 'RE::BSFixedString\\(action\\.execution\\)',
    'RE::BSFixedString\\(""\\)', 'RE::BSFixedString\\(kActionCategory\\)', 'std::int32_t\\{ kActionPriority \\}',
    'RE::BSFixedString\\(""\\)', 'RE::BSFixedString\\(""\\)', 'RE::BSFixedString\\(""\\)'].join(String.raw`\s*,\s*`) + String.raw`\s*\)\);`));
assert.ok(actionArgs && bridgeSource.includes('constexpr std::int32_t kActionPriority = 1;') &&
    bridgeSource.includes('constexpr const char* kActionCategory = "PAPYRUS";'),
    'RegisterAction must be dispatched with all twelve arguments, in order');
assert.ok(bridgeBody('RegisterDecorator').includes('RegisterActions();'), 'the actions are registered right after the decorator, on every load');
// Any integer answer counts as registered; a late answer from an earlier load changes nothing.
const decoratorResult = bridgeBody('OnDecoratorResult');
assert.ok(!/==\s*0/.test(decoratorResult), 'any integer answer from RegisterDecorator counts as registered');
for (const handler of [decoratorResult, bridgeBody('OnActionResult')]) {
    assert.ok(/if \(stamp != _load\.load\(\)\) \{\s*return;/.test(handler), 'a stale answer from an earlier load must be ignored');
}
assert.ok(bridgeBody('RegisterDecorator').includes('_load.fetch_add(1)'), 'each load stamps its dispatches with a new number');
// No C++ action export, anywhere in the plugin.
const srcFiles = execFileSync('git', ['-C', root, 'ls-files', 'src'], { encoding: 'utf8' }).split(/\r?\n/).filter(f => /\.(h|cpp)$/.test(f));
for (const file of srcFiles) {
    const text = readFileSync(join(root, file), 'utf8');
    assert.ok(!text.includes('PublicRegisterCPPAction'), `${file} must not use the C++ action export`);
    if (file !== 'src/romance/RomanceManager.cpp' && file !== 'src/skyrimnet/SkyrimNetBridge.cpp') {
        assert.ok(!text.includes('SkyrimNetBridge::GetSingleton().SendEvent('), `${file} must not send SkyrimNet events`);
    }
}
// The Papyrus side: six functions, none of which refers to SkyrimNetApi.
for (const signature of [
    /Bool Function ConversationEligible\(Actor akActor, Bool abTouched\) Global Native/i,
    /Bool Function ConversationNudge\(Actor akActor, Bool abTouched\) Global Native/i,
    /Bool Function TouchedEligible\(Actor akActor, String contextJson, String paramsJson\) Global/i,
    /Bool Function HurtEligible\(Actor akActor, String contextJson, String paramsJson\) Global/i,
    /Function TouchedExecute\(Actor akActor, String contextJson, String paramsJson\) Global/i,
    /Function HurtExecute\(Actor akActor, String contextJson, String paramsJson\) Global/i]) {
    assert.ok(signature.test(bondScript), `Romantasy_SkyrimNet must declare ${signature.source}`);
}
const scriptFunction = (name) => {
    const found = bondScript.match(new RegExp(`Function ${name}\\([\\s\\S]*?EndFunction`, 'i'));
    assert.ok(found, `Romantasy_SkyrimNet must define ${name}`);
    return found[0];
};
for (const [name, touched] of [['TouchedExecute', 'True'], ['HurtExecute', 'False']]) {
    const text = scriptFunction(name);
    const inside = text.split('\n').slice(1).join('\n');
    assert.ok(new RegExp(`ConversationNudge\\(akActor, ${touched}\\)`).test(inside) && !/contextJson|paramsJson/.test(inside) &&
        (inside.match(/\w+\(/g) || []).length === 1, `${name} must only call ConversationNudge(akActor, ${touched})`);
}
for (const [name, touched] of [['TouchedEligible', 'True'], ['HurtEligible', 'False']]) {
    assert.ok(new RegExp(`Return ConversationEligible\\(akActor, ${touched}\\)`).test(scriptFunction(name)),
        `${name} must answer with ConversationEligible(akActor, ${touched})`);
}
const bondPex = readFileSync(join(root, 'Papyrus', 'Scripts', 'Romantasy_SkyrimNet.pex')).toString('latin1');
for (const name of ['GetBondLine', 'BondDecorator', 'ConversationEligible', 'ConversationNudge', 'TouchedEligible',
    'TouchedExecute', 'HurtEligible', 'HurtExecute']) {
    assert.ok(bondPex.includes(name), `the compiled Romantasy_SkyrimNet.pex must contain ${name}`);
}
assert.ok(!bondPex.includes('SkyrimNetApi'), 'the compiled Romantasy_SkyrimNet.pex must not refer to SkyrimNetApi');
for (const name of ['ConversationEligible', 'ConversationNudge']) {
    const native = papyrusBridgeSource.match(new RegExp(`bool ${name}\\(RE::StaticFunctionTag\\*[\\s\\S]*?\\r?\\n    \\}\\r?\\n`));
    assert.ok(native && /try\s*\{[\s\S]*\}\s*catch\s*\(\.\.\.\)\s*\{\s*return false;/.test(native[0]), `${name} must return false through a catch-all`);
    assert.ok(new RegExp(`RegisterFunction\\("${name}",\\s*kSkyrimNetScriptName,\\s*${name}\\)`).test(papyrusBridgeSource),
        `${name} must be registered on Romantasy_SkyrimNet`);
}
// One const matcher finds a cached bond for every reader.
assert.ok(body('FindCachedFollower').includes('MatchCachedFollower('), 'FindCachedFollower must match through MatchCachedFollower');
assert.ok(bondLineBody[0].includes('MatchCachedFollower(') && !bondLineBody[0].includes(': _followers)'),
    'the bond line must match through MatchCachedFollower, not a copy of it');
const eligible = body('ConversationEligible');
const nudge = body('ConversationNudge');
for (const [name, text] of [['ConversationEligible', eligible], ['ConversationNudge', nudge]]) {
    assert.ok(text.includes('SkyrimNetBridge::GetSingleton().Found()') && text.includes('Settings::GetSingleton().SkyrimNetBridge()') &&
        text.includes('Settings::GetSingleton().SkyrimNetConversation()'), `${name} must test SkyrimNet and both settings`);
    assert.ok(text.includes('CanNudge('), `${name} must ask the conversation limits`);
    assert.ok(text.includes('OutboxLock::Scope lock(_lock);'), `${name} must hold the manager lock`);
    assert.ok(!/Pace\(|ApplyPace\(/.test(text), `${name} must not apply pace`);
}
assert.ok(eligible.includes('MatchCachedFollower(') && !eligible.includes('_viewDirty') &&
    !/FindFollower\(|ObserveCompany\(|EnsureFollowersDiscovered\(|Refresh\w*\(|Noted\(|AddPoints\(/.test(eligible),
    'ConversationEligible changes nothing');
assert.ok(nudge.includes('_viewDirty = true;') && nudge.includes('FindCachedFollower('), 'ConversationNudge is a normal entry point that resolves the bond again');
assert.ok(/AddPoints\(\*follower, delta, label, true, false, true\)/.test(nudge), 'the nudge goes through AddPoints, following not required, gated');
assert.ok(nudge.indexOf('Noted(') > nudge.indexOf('AddPoints(') && nudge.indexOf('Noted(') < nudge.indexOf('if (!applied)'),
    'every attempt that passed the limits is noted, applied or held at the gate');
assert.ok(nudge.indexOf('OutboxLock::Scope') > nudge.indexOf('SkyrimNetConversation()'),
    'with SkyrimNet absent or a switch off the nudge changes nothing, so the checks come before the lock');
assert.ok(nudge.includes('LogWhyNothing(*follower, std::nullopt, 0, romantasy::why::Nothing::Gated);') && nudge.includes('why::Script('),
    'a conversation change writes a why line through the script-award helpers'); 
assert.ok(!nudge.includes('QuietWhispers'), 'a conversation change is a real change and whispers as one');
assert.ok(/"A kind word"/.test(nudge) && /"A harsh word"/.test(nudge) && nudge.includes('kConversationPoints'), 'the nudge uses the exact labels and points');
for (const name of ['Load', 'Revert']) {
    assert.ok(body(name).includes('_conversations.clear();'), `${name} must clear the conversation limits`);
}
assert.ok(body('RemovePlayerFollower').includes('_conversations.erase('), 'removing a bond erases its conversation limits');

// Comfort settings reach the whisper card as optional payload fields, read at show time.
assert.ok(uiBody('PumpWhispers').includes('ComfortFields(settings.HighContrast(), settings.ReduceMotion())'),
    'the pump must give the card the comfort settings at show time');
assert.ok(whisperView.includes('for (const auto& field : comfort) payload[std::string(field.key)] = std::string(field.value);'),
    'WhisperView must add the comfort fields to the whisper payload');
// Each recent change carries its game day, only when it is a finite number of 0 or more.
assert.ok(/if \(std::isfinite\(event\.gameDay\) && event\.gameDay >= 0\.0f\) \{\s*entry\["day"\] = event\.gameDay;\s*\}/.test(stateBuilder[0]),
    'BuildStateJson must send each recent change\'s day when it is finite and not negative');

// The legacy points layouts are in 3.0.1 player saves and have no sample file yet, so pin them here:
// field names, types and order (whitespace-insensitive), and the version test in the legacy points read.
const squash = (text) => text.replace(/\s+/g, ' ').trim();
const structBody = (name) => squash(romanceManager.match(new RegExp(String.raw`struct ${name}\s*\{([^}]*)\}`))[1]);
assert.equal(structBody('SerializedRomancePointsV1'), 'RE::FormID baseFormID = 0; std::int32_t points = 0;', 'the V1 points layout is in a player save');
assert.equal(structBody('SerializedRomancePointsV2'), 'RE::FormID referenceFormID = 0; RE::FormID baseFormID = 0; std::int32_t points = 0;', 'the V2 points layout is in a player save');
assert.ok(romanceManager.includes('if (version != 1 && version != 2)'), 'the legacy points read must accept versions 1 and 2');

// OStim bridge: found through OStim's own SKSE message, never loaded; its headers stay in src/ostim and unchanged;
// the listeners copy plain data and hand off; main.cpp starts it and resets it on every load.
{
    const ostimDir = join(root, 'src', 'ostim');
    const bridge = readFileSync(join(ostimDir, 'OStimBridge.cpp'), 'utf8');
    assert.ok(!bridge.includes('LoadLibrary'), 'OStimBridge.cpp must never load a library');
    assert.ok(/Dispatch\([^;]*"OStim"\)/.test(bridge), 'OStimBridge.cpp must dispatch the interface message to "OStim"');
    assert.ok(bridge.includes('GetModuleHandleW(L"OStim.dll")'), 'OStimBridge.cpp must tell "too old" from "absent" by the loaded module');
    for (const line of ['OStim not found; the bridge is off.', 'OStim found (interface {}); the bridge is on.',
        ' Starting moments needs OStim 7.4 or newer.', "OStim is too old for Romantasy's bridge; it needs 7.3.3 or newer."]) {
        assert.ok(bridge.includes(line), `OStimBridge.cpp must log "${line}" verbatim`);
    }
    for (const name of ['setStartingNodeSafe', 'isChildOf', 'requestInterfaceMap', 'getInferfaceMap']) {
        assert.ok(!bridge.includes(name), `OStimBridge.cpp must not use ${name}`);
    }
    const listeners = [...bridge.matchAll(/void listen\(OStim::Thread\* thread\) override\r?\n    \{([\s\S]*?)\r?\n    \}/g)];
    assert.equal(listeners.length, 3, 'the bridge must have the start, node changed and stop listeners');
    for (const [, listenBody] of listeners) {
        assert.ok(!listenBody.includes('getThread('), 'a listener must never call getThread()');
        assert.ok(!listenBody.includes('RomanceManager'), 'a listener must never reach the manager');
        assert.ok(listenBody.includes('isPlayerThread()'), 'a listener must return unless the thread is the player\'s');
    }
    // The helpers the listeners call (Participants, Copy, ActorForced, Post) live in the file's anonymous namespace.
    const helpers = bridge.match(/\nnamespace\r?\n\{([\s\S]*?)\r?\n\}\r?\n/);
    assert.ok(helpers && ['Participants(', 'Copy(', 'ActorForced(', 'Post('].every((name) => helpers[1].includes(name)),
        'the listener helpers must sit in OStimBridge.cpp\'s anonymous namespace');
    assert.ok(!helpers[1].includes('getThread(') && !helpers[1].includes('RomanceManager'),
        'a listener helper must never call getThread() or reach the manager');
    // A late node from a finished scene cannot seed the next one: no record is created without the player's thread.
    for (const name of ['SceneStarted', 'SceneNode']) {
        const handler = bridge.match(new RegExp(`void OStimBridge::${name}\\([\\s\\S]*?\\r?\\n\\}\\r?\\n`));
        assert.ok(handler && handler[0].includes('(_tracker.Active() || (threads && threads->getThread(0)))') &&
            handler[0].includes('TickerChanged();'), `${name} must create a record only while OStim has the player's thread`);
    }
    const srcDir = join(root, 'src');
    for (const file of readdirSync(srcDir, { recursive: true })) {
        const path = String(file).replaceAll('\\', '/');
        if (!/\.(h|cpp)$/.test(path) || path.startsWith('ostim/')) continue;
        assert.ok(!/ostim[\\/]api[\\/]/i.test(readFileSync(join(srcDir, file), 'utf8')), `${path} must not include OStim's headers`);
    }
    const dataLoaded = mainSource.match(/void OnDataLoaded\(\)[\s\S]*?\r?\n    \}/)[0];
    const loadAt = dataLoaded.indexOf('Settings::GetSingleton().Load()');
    assert.ok(loadAt >= 0 && dataLoaded.indexOf('OStimBridge::GetSingleton().Start()') > loadAt,
        'main.cpp must start the OStim bridge after Settings::Load');
    for (const message of ['kPreLoadGame', 'kNewGame']) {
        const block = mainSource.match(new RegExp(`case SKSE::MessagingInterface::${message}:[\\s\\S]*?break;`));
        assert.ok(block && block[0].includes('OStimBridge::GetSingleton().OnGameReset()'), `main.cpp must reset the OStim bridge at ${message}`);
    }
    const saved = join(root, '.superpowers', 'ostim-ideas', 'ostim-headers', '7.5.2', 'PluginInterface');
    if (existsSync(saved)) {
        const apiDir = join(ostimDir, 'api');
        const copied = readdirSync(apiDir, { recursive: true }).map((f) => String(f)).filter((f) => f.endsWith('.h'));
        assert.equal(copied.length, 25, 'src/ostim/api must hold the 25 copied headers');
        for (const file of copied) {
            assert.ok(readFileSync(join(apiDir, file)).equals(readFileSync(join(saved, file))), `src/ostim/api/${file} must be byte-identical to OStim 7.5.2`);
        }
        assert.ok(readFileSync(join(apiDir, 'LICENSE')).equals(readFileSync(join(saved, '..', 'LICENSE'))), 'src/ostim/api/LICENSE must be OStim\'s licence');
    }
    assert.equal(romanceManager.split('AddTask(').length - 1, 2, 'RomanceManager.cpp must still call AddTask exactly twice');
}

// OStim settings: two switches, on by default, read only when boolean; state JSON carries ostim; health gathers from the bridge.
{
    for (const [key, member] of [['ostimMoments', '_ostimMoments'], ['ostimHold', '_ostimHold']]) {
        assert.ok(settingsSource.includes(`ReadSwitch(j, "${key}", ${member});`), `Settings::Load must read ${key} through ReadSwitch`);
        assert.ok(settingsSource.includes(`j["${key}"]`), `Settings::Save must write ${key}`);
        assert.ok(new RegExp(`std::atomic_bool ${member} = true;`).test(settingsHeader), `${key} must default to on`);
    }
    assert.ok(/\{\s*"ostim",\s*\{\s*\{\s*"found",\s*OStimBridge::GetSingleton\(\)\.Found\(\)\s*\},\s*\{\s*"moments",\s*Settings::GetSingleton\(\)\.OStimMoments\(\)\s*\},\s*\{\s*"hold",\s*Settings::GetSingleton\(\)\.OStimHold\(\)\s*\}\s*\}\s*\}/.test(stateSource),
        'BuildStateJson must send ostim { found, moments, hold }');
    assert.ok(/facts\.ostim = ostim\.Found\(\) \? OStimState::Found : ostim\.TooOld\(\) \? OStimState::TooOld : OStimState::Absent;/.test(healthGather),
        'the health check must gather the OStim state from OStimBridge');
    assert.ok(healthGather.includes('facts.ostimMoments = settings.OStimMoments();'), 'the health check must gather the Shared moments switch');
}

// Quiet during scenes: tier changes and whispers wait while the player is in an OStim scene and the hold is on.
{
    assert.ok(body('ReleaseTierChanges').includes('.inScene = SceneHolds()'), 'ReleaseTierChanges must fill .inScene = SceneHolds()');
    for (const name of ['QueueOrShowLevelChange', 'FlushPendingLevelChanges']) {
        assert.ok(body(name).includes('SceneHolds()'), `${name} must hold while SceneHolds()`);
    }
    assert.ok(/OStimBridge::GetSingleton\(\)\.InScene\(\) && Settings::GetSingleton\(\)\.OStimHold\(\)/.test(body('SceneHolds')),
        'SceneHolds must read the bridge and the hold setting');
    const sceneEnded = body('OnSceneEnded');
    const flushAt = sceneEnded.indexOf('FlushPendingLevelChanges();');
    assert.ok(flushAt >= 0 && sceneEnded.indexOf('ReleaseTierChanges(false);') > flushAt,
        'OnSceneEnded must call FlushPendingLevelChanges and then ReleaseTierChanges(false)');
    const uiSource = readFileSync(join(root, 'src', 'ui', 'RomantasyUI.cpp'), 'utf8');
    const canShow = uiSource.match(/RomantasyUI::CanShowWhispers\([\s\S]*?\r?\n\}\r?\n/);
    assert.ok(canShow && canShow[0].includes('OStimBridge::GetSingleton().InScene()') && canShow[0].includes('OStimHold()'),
        'CanShowWhispers must read OStimBridge and OStimHold');
}

// Shared moments: counted when the player's scene ends, at most once a day, through AddPoints; read and tested from Papyrus.
{
    for (const declaration of [
        'Float Function GetDaysSinceMoment(Actor akFollower) Native Global',
        'Int Function GetMomentCount(Actor akFollower) Native Global',
        'Bool Function DebugSharedMoment(Actor akFollower, String asKind, Float afSeconds = 60.0, Bool abOthers = False, Bool abForce = False) Native Global',
    ]) {
        assert.ok(papyrusScript.split(/\r?\n/).includes(declaration), `Romantasy.psc must declare ${declaration}`);
    }
    for (const name of ['GetDaysSinceMoment', 'GetMomentCount', 'DebugSharedMoment']) {
        assert.ok(papyrusBridge.includes(`RegisterFunction("${name}", kScriptName, ${name})`), `PapyrusBridge.cpp must register ${name}`);
    }
    const debugMoment = papyrusBridge.match(/\n    \S[^\n(]* DebugSharedMoment\(\s*RE::StaticFunctionTag\*[^)]*\)\s*\{\s*([^\n]*)/);
    assert.ok(debugMoment && debugMoment[1] === 'if (!Settings::GetSingleton().AuthorMode()) {',
        'DebugSharedMoment must check AuthorMode() before anything else');
    const award = body('AwardMoment');
    assert.ok(/AddPoints\(follower, verdict\.points, [^;]*, true, false, true\)/.test(award),
        'AwardMoment must go through AddPoints, following not required, gated');
    const beginAt = award.indexOf('journal::Begin(');
    assert.ok(beginAt >= 0 && award.indexOf('Noted(') > beginAt, 'AwardMoment must begin the journal before noting the moment');
    assert.ok(award.includes('"Romantasy_OnSharedMoment"'), 'AwardMoment must send Romantasy_OnSharedMoment');
    assert.ok(!/ApplyPace|Pace\(/.test(award), 'a shared moment is never paced');
    assert.ok(award.includes('why::Moment(') && award.includes('why::MomentNothing('), 'AwardMoment must write its why lines');
    assert.ok(award.includes('romantasy::why::NothingText(romantasy::why::Nothing::Gated,') && !award.includes('LogWhyNothing('),
        'a gain the gate held is logged as a shared moment with the gate reason text');
    const notCounted = award.match(/if \(!verdict\.counted\) \{[\s\S]*?return false;\s*\}/);
    assert.ok(notCounted && notCounted.index < beginAt, 'AwardMoment must return for a moment not counted before it begins the journal');
    assert.ok(/SendBarkEvent\(_lock, "Romantasy_OnSharedMoment", std::string\(romantasy::ostim::Key\(facts\.kind\)\)\.c_str\(\),\s*static_cast<float>\(applied\), senderForm\);/.test(award),
        'Romantasy_OnSharedMoment must carry the kind key and the applied points');
    const sceneEnded = body('OnSceneEnded');
    for (const text of ['.participants = static_cast<std::uint32_t>(scene.participants.size())',
        '.bonded = static_cast<std::uint32_t>(bonds.size())', 'IsBondSwitchedOn(']) {
        assert.ok(sceneEnded.includes(text), `OnSceneEnded must contain ${text}`);
    }
    const playerSkipAt = sceneEnded.search(/if \(id == kPlayer\) \{\s*playerPresent = true;\s*continue;/);
    assert.ok(sceneEnded.includes('constexpr RE::FormID kPlayer = 0x14;') && playerSkipAt >= 0 &&
        playerSkipAt < sceneEnded.indexOf('MatchCachedFollower('), 'OnSceneEnded must skip the player (0x14) before matching bonds');
    const lostAt = sceneEnded.indexOf('scene.lost');
    const judgeAt = sceneEnded.indexOf('AwardMoment(');
    assert.ok(lostAt >= 0 && judgeAt > lostAt, 'OnSceneEnded must test scene.lost before judging');
    const flushAt = sceneEnded.indexOf('FlushPendingLevelChanges();');
    assert.ok(flushAt > judgeAt && sceneEnded.indexOf('ReleaseTierChanges(false);') > flushAt,
        'OnSceneEnded must still end with the flush and the release');
    for (const name of ['GetMomentCount', 'GetDaysSinceMoment']) {
        const getter = body(name);
        assert.ok(getter.includes('OutboxLock::Scope lock(_lock);') && getter.includes('FindMoments(') && getter.includes('MatchCachedFollower(') &&
            !/MomentsFor\(|FindFollower\(|_viewDirty|AddPoints\(|Noted\(/.test(getter), `${name} must read under the lock and change nothing`);
    }
    const debugShared = body('DebugSharedMoment');
    assert.ok(debugShared.includes('ParseKind(') && debugShared.includes('AwardMoment(') && debugShared.includes('_viewDirty = true;'),
        'DebugSharedMoment must parse the kind and run AwardMoment');
    assert.ok(!/OStimBridge|SceneTracker/.test(debugShared), 'DebugSharedMoment must work without OStim');
    const pex = readFileSync(join(root, 'Papyrus', 'Scripts', 'Romantasy.pex')).toString('latin1');
    for (const name of ['GetDaysSinceMoment', 'GetMomentCount', 'DebugSharedMoment']) {
        assert.ok(pex.includes(name), `the compiled Romantasy.pex must contain ${name}`);
    }
}

// Starting a moment: TryShareMoment claims a start before it defers the begin; CanShareMoment only answers. The bridge
// asks OStim by name through the VM, watches the scene, stops a wrong one, and forgets a pending start in time.
{
    for (const declaration of [
        'Int Function TryShareMoment(Actor akFollower, String asKind = "") Native Global',
        'Int Function CanShareMoment(Actor akFollower, String asKind = "") Native Global',
    ]) {
        assert.ok(papyrusScript.split(/\r?\n/).includes(declaration), `Romantasy.psc must declare ${declaration}`);
    }
    for (const name of ['TryShareMoment', 'CanShareMoment']) {
        assert.ok(papyrusBridge.includes(`RegisterFunction("${name}", kScriptName, ${name})`), `PapyrusBridge.cpp must register ${name}`);
    }
    assert.ok(papyrusBridge.includes('constexpr std::int32_t kApiVersion = 9;'), 'the start functions arrive with API version 9');
    const tryShare = body('TryShareMoment');
    const claimAt = tryShare.indexOf('ClaimStart()');
    const deferAt = tryShare.indexOf('_lock.Defer(');
    assert.ok(claimAt >= 0 && deferAt > claimAt, 'TryShareMoment must claim the start before it defers the begin');
    assert.ok(tryShare.includes('BeginMoment(handle, startKind, stamp)'), 'TryShareMoment must defer BeginMoment with the stamp of its claim');
    const stampAt = tryShare.indexOf('ClaimedStamp()');
    assert.ok(stampAt > claimAt && stampAt < deferAt, 'TryShareMoment must read the stamp after the claim and before the defer');
    assert.ok(tryShare.includes('"Romantasy_OnMomentRefused"'), 'TryShareMoment must send Romantasy_OnMomentRefused');
    assert.ok(tryShare.includes('RaiseWhisper(') && tryShare.includes('Kind::MomentNotYet') && tryShare.includes('Kind::MomentNotInMood'),
        'TryShareMoment must whisper Not yet and Not in the mood');
    assert.ok(tryShare.includes('_viewDirty = true;'), 'TryShareMoment must set _viewDirty');
    assert.ok(!tryShare.includes('AddTask('), 'TryShareMoment must never call AddTask');
    const canShare = body('CanShareMoment');
    assert.ok(!/Defer\(|RaiseWhisper\(|SendBarkEvent\(|ClaimStart\(|_viewDirty/.test(canShare), 'CanShareMoment must change nothing');
    assert.ok(canShare.includes('CheckMomentStart(') && tryShare.includes('CheckMomentStart('), 'both functions must answer through CheckMomentStart');
    const checkStart = body('CheckMomentStart');
    assert.ok(checkStart.includes('MatchCachedFollower(') && checkStart.includes('IsBondSwitchedOn(') && checkStart.includes('CheckStart(facts)') &&
        checkStart.includes('StartPending()') && checkStart.includes('IsPlayerInCombat()') && !checkStart.includes('FindFollower('),
        'CheckMomentStart must read the cached bond, the bridge and combat, and run CheckStart');
    const couldNot = body('OnMomentCouldNotStart');
    assert.ok(couldNot.includes('OutboxLock::Scope lock(_lock);') && couldNot.includes('"Romantasy_OnMomentRefused"') && !couldNot.includes('RaiseWhisper('),
        'OnMomentCouldNotStart must send Romantasy_OnMomentRefused under the lock, without a whisper');

    const bridge = readFileSync(join(root, 'src', 'ostim', 'OStimBridge.cpp'), 'utf8');
    for (const text of ['"GetRandomSceneSuperloadCSV"', '"OLibrary"', '"OThread"', '"Stop"', 'setDuration(30000)', 'noPlayerControl()', 'noAutoMode()',
        'noUndressing()', 'noFurniture()', 'setStartingNode(', 'createThreadBuilder(']) {
        assert.ok(bridge.includes(text), `OStimBridge.cpp must contain ${text}`);
    }
    assert.ok(!bridge.includes('setStartingNodeSafe'), 'OStimBridge.cpp must never call setStartingNodeSafe');
    for (const ms of ['5000', '40000', '60000']) {
        assert.ok(new RegExp(`constexpr std::uint64_t k\\w+Ms = ${ms};`).test(bridge), `OStimBridge.cpp must name the ${ms} ms limit`);
    }
    assert.ok(/kSuperloadArguments = 46;/.test(bridge), 'the scene search must pack all 46 arguments');
    const begin = bridge.match(/void OStimBridge::BeginMoment\([\s\S]*?\r?\n\}\r?\n/);
    assert.ok(begin && begin[0].includes('stamp != _startStamp.load()'), 'BeginMoment must do nothing for an older claim');
    const reset = bridge.match(/void OStimBridge::OnGameReset\(\)[\s\S]*?\r?\n\}\r?\n/);
    assert.ok(reset && reset[0].includes('EndPending('), 'OnGameReset must clear the pending start');
    const node = bridge.match(/void OStimBridge::SceneNode\([\s\S]*?\r?\n\}\r?\n/);
    assert.ok(node && node[0].includes('StopScene(') && node[0].includes('OnMomentCouldNotStart('),
        'a scene of ours that leaves its kind must be stopped and reported as code 6');
    const tick = bridge.match(/void OStimBridge::OnTick\(\)[\s\S]*?\r?\n\}\r?\n/);
    assert.ok(tick && tick[0].includes('OursGentleFor(') && tick[0].includes('StopScene('), 'the ticker must stop a gentle scene of ours at the fallback');
    const ticker = bridge.match(/void OStimBridge::TickerChanged\(\)[\s\S]*?\r?\n\}\r?\n/);
    assert.ok(ticker && ticker[0].includes('StartPending()'), 'the ticker must run while a start is pending');
    const bridgeHeader = readFileSync(join(root, 'src', 'ostim', 'OStimBridge.h'), 'utf8');
    for (const text of ['[[nodiscard]] bool StartPending() const;', 'bool ClaimStart();', '[[nodiscard]] std::uint32_t ClaimedStamp() const;', 'void BeginMoment(RE::ActorHandle follower, romantasy::ostim::Kind kind, std::uint32_t stamp);']) {
        assert.ok(bridgeHeader.includes(text), `OStimBridge.h must declare ${text}`);
    }

    const scripts = git('-C', root, 'ls-files', '*.psc').split(/\r?\n/).filter(Boolean);
    assert.ok(scripts.length > 0, 'the repository must track its Papyrus sources');
    for (const file of scripts) {
        const text = readFileSync(join(root, file), 'utf8');
        for (const name of ['OLibrary', 'OThread', 'OStim']) {
            assert.ok(!text.includes(name), `${file} must not name ${name}`);
        }
    }
    // OStim waits for dialogue to close before a player scene starts: the forget clock holds while it is open.
    assert.ok(tick && /IsMenuOpen\(RE::DialogueMenu::MENU_NAME\)\) \{\s*_pending->sinceMs = now;/.test(tick[0]),
        'the pending start must not be forgotten while the Dialogue menu is open');
    assert.ok(!/searchSinceMs = now/.test(tick[0]), 'dialogue must not extend the scene search timeout');
    // InScene going down with no record still releases what the scene held.
    assert.ok(/if \(!end\) \{[\s\S]*?end->lost = true;[\s\S]*?OnSceneEnded\(\*end\);/.test(tick[0]),
        'a lost InScene with no record must still call OnSceneEnded with a lost end');
    const sceneEndedWhy = body('OnSceneEnded');
    assert.ok(sceneEndedWhy.includes('SceneNothing("the scene was lost")') && sceneEndedWhy.includes('SceneNothing("no switched-on bond in it")'),
        'OnSceneEnded must write a why line for a lost scene and for a scene with no bond');
    assert.ok(body('BuildJournalJson').includes('FindMoments(*follower)') && !body('BuildJournalJson').includes('_moments.find('),
        'BuildJournalJson must find the moments through FindMoments');
    const pex = readFileSync(join(root, 'Papyrus', 'Scripts', 'Romantasy.pex')).toString('latin1');
    for (const name of ['TryShareMoment', 'CanShareMoment']) {
        assert.ok(pex.includes(name), `the compiled Romantasy.pex must contain ${name}`);
    }
}

console.log('Runtime compatibility contract tests passed.');
