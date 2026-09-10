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
