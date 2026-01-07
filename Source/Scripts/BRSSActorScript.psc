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

    If IsSlave()
        ForceAV("Health", 1.0)
    EndIf

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

Bool Function IsGuard()
    Return IsInFaction(BRSS_Guards)
EndFunction

Bool Function IsSlave()
    Return IsInFaction(BRSS_Slaves)
EndFunction

Function Wait(Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, None)
    SetLinkedRef(BRSS_PackageKeyword2, None)
    SetAV("Variable08", 0)
    EvaluatePackage()

    ReleaseLock(bypassLock)
EndFunction

Function Travel(ObjectReference target, Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, target)
    SetLinkedRef(BRSS_PackageKeyword2, None)
    SetAV("Variable08", 1)
    EvaluatePackage()

    ReleaseLock(bypassLock)
EndFunction

Function Follow(ObjectReference target, Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, target)
    SetLinkedRef(BRSS_PackageKeyword2, None)
    SetAV("Variable08", 2)
    EvaluatePackage()

    ReleaseLock(bypassLock)
EndFunction

Function UseIdleMarker(ObjectReference target, ObjectReference secondTarget=None, Bool bypassLock=False)
    AcquireLock(bypassLock)

    If target
        SetLinkedRef(BRSS_PackageKeyword1, target)
    EndIf
    If secondTarget
        SetLinkedRef(BRSS_PackageKeyword2, secondTarget)
    EndIf
    SetAV("Variable08", 3)
    EvaluatePackage()

    ReleaseLock(bypassLock)
EndFunction

Function UseWeapon(ObjectReference target, ObjectReference loc, Bool bypassLock=False)
    AcquireLock(bypassLock)

    If loc
        SetLinkedRef(BRSS_PackageKeyword1, loc)
    EndIf
    If target
        SetLinkedRef(BRSS_PackageKeyword2, target)
    EndIf
    SetAV("Variable08", 4)
    EvaluatePackage()

    ReleaseLock(bypassLock)
EndFunction

Function ToggleUseWeapon()
    AcquireLock()

    ObjectReference slot1 = GetLinkedRef(BRSS_PackageKeyword1)
    ObjectReference slot2 = GetLinkedRef(BRSS_PackageKeyword2)

    If IsGuard()
        If IsUsingIdleMarker() && slot2
            Aim(slot2, slot1, bypassLock=True)
        ElseIf IsAiming()
            UseWeapon(slot2, slot1, bypassLock=True)
        ElseIf IsUsingWeapon()
            UseIdleMarker(slot1, slot2, bypassLock=True)
        EndIf
    EndIf

    ReleaseLock()
EndFunction

Function Patrol(ObjectReference p1, ObjectReference p2, Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, p1)
    SetLinkedRef(BRSS_PackageKeyword2, p2)
    SetAV("Variable08", 6)
    EvaluatePackage()

    ReleaseLock(bypassLock)
EndFunction

Function Aim(ObjectReference target, ObjectReference loc, Bool bypassLock=False)
    AcquireLock(bypassLock)

    If loc
        SetLinkedRef(BRSS_PackageKeyword1, loc)
    EndIf
    If target
        SetLinkedRef(BRSS_PackageKeyword2, target)
    EndIf
    SetAV("Variable08", 7)
    EvaluatePackage()

    ReleaseLock(bypassLock)
EndFunction

Function Sit(ObjectReference target, Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, target)
    SetLinkedRef(BRSS_PackageKeyword2, None)
    SetAV("Variable08", 8)
    EvaluatePackage()

    ReleaseLock(bypassLock)
EndFunction

Function UseWeaponOnce(ObjectReference target, ObjectReference loc, Bool bypassLock=False)
    AcquireLock(bypassLock)

    If loc
        SetLinkedRef(BRSS_PackageKeyword1, loc)
    EndIf
    If target
        SetLinkedRef(BRSS_PackageKeyword2, target)
    EndIf
    SetAV("Variable08", 9)
    EvaluatePackage()

    ReleaseLock(bypassLock)
EndFunction

Function Use(ObjectReference target, ObjectReference secondTarget=None, Bool bypassLock=False)
    AcquireLock(bypassLock)

    If target.GetBaseObject() as Furniture
        Sit(target, bypassLock=True)
    Else
        UseIdleMarker(target, secondTarget, bypassLock=True)
    EndIf

    ReleaseLock(bypassLock)
EndFunction

Bool Function IsWaiting()
    Return GetAV("Variable08") as Int == 0
EndFunction

Bool Function IsTraveling()
    Return GetAV("Variable08") as Int == 1
EndFunction

Bool Function IsFollowing()
    Return GetAV("Variable08") as Int == 2
EndFunction

Bool Function IsUsingIdleMarker()
    Return GetAV("Variable08") as Int == 3
EndFunction

Bool Function IsUsingWeapon()
    Return GetAV("Variable08") as Int == 4
EndFunction

Bool Function IsPatrolling()
    Return GetAV("Variable08") as Int == 6
EndFunction

Bool Function IsAiming()
    Return GetAV("Variable08") as Int == 7
EndFunction

Bool Function IsSitting()
    Return GetAV("Variable08") as Int == 8
EndFunction

Bool Function IsUsingWeaponOnce()
    Return GetAV("Variable08") as Int == 9
EndFunction

Function SetActorName(String value)
    If ! value
        Return
    EndIf

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

    Float distance = GetDistance(Game.GetPlayer())
    If distance > 1000000.0 || distance < 0.0
        distance = -1.0
    EndIf

    String procedure = "No action assigned"
    Int procedureCode = GetAV("Variable08") as Int
    ; TODO: Improve this
    If procedureCode == 0
        procedure = "Idle"
    ElseIf procedureCode == 1
        procedure = "Traveling to " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword1))
    ElseIf procedureCode == 2
        procedure = "Following " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword1))
    ElseIf procedureCode == 3
        procedure = "Using Idle Marker " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword1)) + " with target " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword2))
    ElseIf procedureCode == 4 || procedureCode == 9
        procedure = "Using weapon on " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword2))
    ElseIf procedureCode == 6
        procedure = "Patrolling between " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword1)) + " and " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword2))
    ElseIf procedureCode == 7
        procedure = "Aiming at " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword2))
    ElseIf procedureCode == 8
        procedure = "Sitting at " + BRSSLogger.GetLogID(GetLinkedRef(BRSS_PackageKeyword1))
    EndIf

    Return                                                                             \
        GetDisplayName()                                                              +\
        "["                                                                           +\
        BRSSUtil.GetFormID(Self)                                               + ", " +\
        type                                                                   + ", " +\
        aliveStatus                                                            + ", " +\
        GetCurrentLocation().GetName() + "(" + distance + ")"                         +\
        "] "                                                                          +\
        procedure
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
