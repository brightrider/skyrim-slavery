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

String Name
ObjectReference[] LinkedRefs

Bool Lock = False

Event OnInit()
    Name = GetDisplayName()
    LinkedRefs = new ObjectReference[2]

    RegisterForModEvent("BRSSGameLoaded", "OnGameLoaded")

    IgnoreFriendlyHits()

    BRSSLogger.LogInfo("BRSSActor", Self, "Initialized")
EndEvent

Event OnGameLoaded(String eventName, String strArg, Float numArg, Form sender)
    AcquireLock()

    PO3_SKSEFunctions.SetLinkedRef(Self, LinkedRefs[0], BRSS_PackageKeyword1)
    PO3_SKSEFunctions.SetLinkedRef(Self, LinkedRefs[1], BRSS_PackageKeyword2)

    SetDisplayName(Name)

    BRSSLogger.LogInfo("BRSSActor", Self, "Initialized (Game Loaded)")

    ReleaseLock()
EndEvent

Function Wait(Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, None)
    SetLinkedRef(BRSS_PackageKeyword2, None)
    SetAV("Variable08", 0)
    EvaluatePackage()

    BRSSLogger.LogInfo("BRSSActor", Self, "Executing WAIT procedure")

    ReleaseLock(bypassLock)
EndFunction

Function Travel(ObjectReference target, Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, target)
    SetLinkedRef(BRSS_PackageKeyword2, None)
    SetAV("Variable08", 1)
    EvaluatePackage()

    BRSSLogger.LogInfo("BRSSActor", Self, "Executing TRAVEL procedure with target: " + BRSSLogger.GetLogID(target))

    ReleaseLock(bypassLock)
EndFunction

Function Follow(ObjectReference target, Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, target)
    SetLinkedRef(BRSS_PackageKeyword2, None)
    SetAV("Variable08", 2)
    EvaluatePackage()

    BRSSLogger.LogInfo("BRSSActor", Self, "Executing FOLLOW procedure with target: " + BRSSLogger.GetLogID(target))

    ReleaseLock(bypassLock)
EndFunction

Function UseIdleMarker(ObjectReference target, Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, target)
    SetLinkedRef(BRSS_PackageKeyword2, None)
    SetAV("Variable08", 3)
    EvaluatePackage()

    BRSSLogger.LogInfo("BRSSActor", Self, "Executing USE_IDLE_MARKER procedure with target: " + BRSSLogger.GetLogID(target))

    ReleaseLock(bypassLock)
EndFunction

Function SetActorName(String value)
    Name = value
    SetDisplayName(value)
EndFunction

String Function GetDescription()
    String type = "Guard"
    If IsInFaction(BRSS_Slaves)
        type = "Slave"
    EndIf

    String aliveStatus = "Alive"
    If IsDead()
        aliveStatus = "Dead"
    EndIf

    String procedure = "No action assigned"
    Int procedureCode = GetAV("Variable08") as Int
    If procedureCode == 0
        procedure = "Idle"
    ElseIf procedureCode == 1
        procedure = "Traveling to " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword1))
    ElseIf procedureCode == 2
        procedure = "Following " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword1))
    ElseIf procedureCode == 3
        procedure = "Using Idle Marker " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword1))
    EndIf

    Return GetDisplayName() + "[" + BRSSUtil.GetFormID(Self) + ", " + type + ", " + aliveStatus + "] " + procedure
EndFunction

; ##############################################################################
; # Private Functions
; ##############################################################################
Function SetLinkedRef(Keyword kwd, ObjectReference target)
    If kwd == BRSS_PackageKeyword1
        LinkedRefs[0] = target
    ElseIf kwd == BRSS_PackageKeyword2
        LinkedRefs[1] = target
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
