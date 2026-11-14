; SPDX-License-Identifier: MIT

Scriptname Romantasy Hidden

Int Function GetApiVersion() Native Global

Bool Function ModifyPoints(Actor akFollower, Int aiPoints, String asReason = "", Bool abShowLevelUp = True) Native Global

Bool Function ApplyPreference(Actor akFollower, String asStatName, Int aiDelta = 1, Bool abShowLevelUp = True) Native Global

Bool Function ClearPreferences(Actor akFollower) Native Global

Bool Function SetPreference(Actor akFollower, String asStatName, Int aiDirection) Native Global

Int Function GetPreference(Actor akFollower, String asStatName) Native Global

Bool Function SetPreferencesManual(Actor akFollower, Bool abManual = True) Native Global

Bool Function IsPreferencesManual(Actor akFollower) Native Global

Bool Function RegisterAuthorFollower(Actor akFollower, String[] asStatNames, Int[] aiDirections, Int aiStartingLevel = 1, GlobalVariable akRelationshipRankMirror = None) Native Global

Bool Function RegisterAuthorFollowerPointsMirror(Actor akFollower, GlobalVariable akRelationshipPointsMirror) Native Global

Int Function GetPoints(Actor akFollower) Native Global

Int Function GetLevel(Actor akFollower) Native Global

String Function GetLevelName(Actor akFollower) Native Global

; The bond's mood: 1 Strained, 2 Settled, 3 Warm, 0 when Romantasy does not manage the actor.
Int Function GetMood(Actor akFollower) Native Global

; The mood's name: "Strained", "Settled" or "Warm"; empty when Romantasy does not manage the actor.
String Function GetMoodName(Actor akFollower) Native Global

; How many shared moments the bond has had; 0 when Romantasy does not manage the actor.
Int Function GetMomentCount(Actor akFollower) Native Global

; In-game days since the last shared moment; -1.0 when there has been none or Romantasy does not manage the actor.
Float Function GetDaysSinceMoment(Actor akFollower) Native Global

; Starts a shared moment with a bond. asKind is "affection", "closeness", "intimacy", or "" for the deepest kind the
; bond welcomes now. Following is not required. Returns:
; 0 accepted: Romantasy is starting it
; 1 not available: no scene framework, one too old to start scenes, or an unknown kind
; 2 not a bond: no bond with this actor, or it is switched off
; 3 too soon: the bond's tier is below the kind's welcome tier
; 4 strained: the follower's mood is Strained
; 5 not now: in combat, in a scene, or a start is already waiting
; Codes 3 and 4 also send Romantasy_OnMomentRefused (strArg = the kind, numArg = the code). With asKind "" and no
; kind welcome at all, strArg is "". When an accepted moment cannot begin after all, the same event comes later with
; code 6, unless the follower is no longer loaded by then.
Int Function TryShareMoment(Actor akFollower, String asKind = "") Native Global

; The same answer as TryShareMoment, without starting anything.
Int Function CanShareMoment(Actor akFollower, String asKind = "") Native Global

; Author test functions. They do nothing (False or "") unless settings.json has "authorMode": true.

; Runs one built-in deed for this follower as if the player had done it. The follower need not be following.
Bool Function DebugDeed(Actor follower, String statistic, Int count = 1) Native Global

; Sets the follower's points. Tier changes are announced as usual.
Bool Function DebugSetPoints(Actor follower, Int points) Native Global

; Sets the days the follower has spent with the player, 0 to 100000.
Bool Function DebugSetDaysTogether(Actor follower, Float days) Native Global

; Sets the mood score, -100 to 100.
Bool Function DebugSetMood(Actor follower, Int score) Native Global

; Announces every waiting tier change now, as a calm moment would.
Bool Function DebugAnnounceNow() Native Global

; One line about the bond: points, tier, mood, days together, gate, personality and preferences. Also logged.
String Function DebugDescribe(Actor follower) Native Global

; Judges a shared moment ("affection", "closeness" or "intimacy") as the end of a scene would. Works with no scene framework installed.
Bool Function DebugSharedMoment(Actor akFollower, String asKind, Float afSeconds = 60.0, Bool abOthers = False, Bool abForce = False) Native Global
