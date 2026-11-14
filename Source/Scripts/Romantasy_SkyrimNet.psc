; SPDX-License-Identifier: MIT

Scriptname Romantasy_SkyrimNet Hidden

; The bond line Romantasy gives SkyrimNet for an actor, or "" when there is nothing to say.
String Function GetBondLine(Actor akActor) Native Global

; Registered with SkyrimNet as the decorator romantasy_bond. It never calls SkyrimNet, so it loads without it.
String Function BondDecorator(Actor akActor) Global
	If akActor == None
		Return ""
	EndIf
	Return GetBondLine(akActor)
EndFunction

; Whether SkyrimNet may offer the actor RomantasyTouched (abTouched) or RomantasyHurt now. Changes nothing.
Bool Function ConversationEligible(Actor akActor, Bool abTouched) Global Native

; Moves the actor's bond by a few capped points for RomantasyTouched (abTouched) or RomantasyHurt.
Bool Function ConversationNudge(Actor akActor, Bool abTouched) Global Native

; Registered with SkyrimNet as the eligibility and execution functions of RomantasyTouched and RomantasyHurt.
; contextJson and paramsJson are never read.
Bool Function TouchedEligible(Actor akActor, String contextJson, String paramsJson) Global
	If akActor == None
		Return False
	EndIf
	Return ConversationEligible(akActor, True)
EndFunction

Function TouchedExecute(Actor akActor, String contextJson, String paramsJson) Global
	If akActor != None
		ConversationNudge(akActor, True)
	EndIf
EndFunction

Bool Function HurtEligible(Actor akActor, String contextJson, String paramsJson) Global
	If akActor == None
		Return False
	EndIf
	Return ConversationEligible(akActor, False)
EndFunction

Function HurtExecute(Actor akActor, String contextJson, String paramsJson) Global
	If akActor != None
		ConversationNudge(akActor, False)
	EndIf
EndFunction
