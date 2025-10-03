Scriptname BRSSActorScript extends Actor

Faction Property BRSS_Actors Auto
Faction Property BRSS_Guards Auto
Faction Property BRSS_Slaves Auto

Package Property BRSS_Guard_Wait Auto
Package Property BRSS_Guard_Travel Auto
Package Property BRSS_Guard_Follow Auto
Package Property BRSS_Guard_UseIdleMarker Auto
Package Property BRSS_Guard_UseWeapon Auto
Package Property BRSS_Guard_UseMagic Auto
Keyword Property BRSS_PackageKeyword1 Auto
Keyword Property BRSS_PackageKeyword2 Auto

ObjectReference[] LinkedRefs

Bool Lock = False

Event OnInit()
    LinkedRefs = new ObjectReference[3]

    RegisterForModEvent("BRSSGameLoaded", "OnGameLoaded")

    IgnoreFriendlyHits()
EndEvent

Event OnGameLoaded(String eventName, String strArg, Float numArg, Form sender)
    PO3_SKSEFunctions.SetLinkedRef(Self, LinkedRefs[0], None)
    PO3_SKSEFunctions.SetLinkedRef(Self, LinkedRefs[1], BRSS_PackageKeyword1)
    PO3_SKSEFunctions.SetLinkedRef(Self, LinkedRefs[2], BRSS_PackageKeyword2)
EndEvent

; ##############################################################################
; # Private Functions
; ##############################################################################
Function SetLinkedRef(Keyword kwd, ObjectReference target)
    If kwd == None
        LinkedRefs[0] = target
    ElseIf kwd == BRSS_PackageKeyword1
        LinkedRefs[1] = target
    ElseIf kwd == BRSS_PackageKeyword2
        LinkedRefs[2] = target
    EndIf
    PO3_SKSEFunctions.SetLinkedRef(Self, target, kwd)
EndFunction

Function AcquireLock(Bool bypass=False, Float spinDelay=0.001)
    If bypass
        Return
    EndIf

    While Lock
        Utility.Wait(spinDelay)
    EndWhile
    Lock = True
EndFunction

Function ReleaseLock(Bool bypass=False)
    If bypass
        Return
    EndIf

    Lock = False
EndFunction
; ##############################################################################
