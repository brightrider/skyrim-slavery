Scriptname BRSSActorScript extends Actor

Faction Property BRSS_Actors Auto
Faction Property BRSS_Guards Auto
Faction Property BRSS_Guards_Trader Auto
Faction Property BRSS_Slaves Auto
Faction Property BRSS_Slaves_Dead Auto

Keyword Property BRSS_PackageKeyword1 Auto
Keyword Property BRSS_PackageKeyword2 Auto

Keyword Property FurnitureWoodChoppingBlock Auto
MiscObject Property BYOHMaterialClay Auto
MiscObject Property BYOHMaterialStoneBlock Auto
MiscObject Property Firewood01 Auto
MiscObject Property OreCorundum Auto
MiscObject Property OreEbony Auto
MiscObject Property OreGold Auto
MiscObject Property OreIron Auto
MiscObject Property OreMalachite Auto
MiscObject Property OreMoonstone Auto
MiscObject Property OreOrichalcum Auto
MiscObject Property OreQuicksilver Auto
MiscObject Property OreSilver Auto

String Name
ObjectReference[] LinkedRefs

MiscObject MiningResource

Bool Lock = False

Event OnInit()
    Name = GetDisplayName()
    LinkedRefs = new ObjectReference[2]

    RegisterForModEvent("BRSSGameLoaded", "OnGameLoaded")

    IgnoreFriendlyHits()

    If IsSlave()
        ForceAV("Health", 1000000000.0)
    EndIf
EndEvent

Event OnGameLoaded(String eventName, String strArg, Float numArg, Form sender)
    AcquireLock()

    PO3_SKSEFunctions.SetLinkedRef(Self, LinkedRefs[0], BRSS_PackageKeyword1)
    PO3_SKSEFunctions.SetLinkedRef(Self, LinkedRefs[1], BRSS_PackageKeyword2)

    SetDisplayName(Name)

    ReleaseLock()
EndEvent

Event OnActivate(ObjectReference akActionRef)
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript

    If IsInFaction(BRSS_Guards_Trader)
        controller.BuyActor()
    EndIf
EndEvent

Event OnItemAdded(Form akBaseItem, int aiItemCount, ObjectReference akItemReference, ObjectReference akSourceContainer)
    EquipItem(akBaseItem, abPreventRemoval=True, abSilent=True)
EndEvent

Event OnUpdate()
    AcquireLock()

    If IsGuard()
        If IsUsingIdleMarkerWeaponDrawn() || IsAiming()
            Debug.SendAnimationEvent(Self, "AttackStart")
            UnregisterForUpdate()
            RegisterForSingleUpdate(10.0)
        EndIf
    EndIf

    ReleaseLock()
EndEvent

Event OnUpdateGameTime()
    AcquireLock()

    If IsSlave()
        If !IsUsingIdleMarker() && !IsSitting()
            ReleaseLock()
            Return
        EndIf

        ObjectReference target = GetLinkedRef(BRSS_PackageKeyword1)
        If MiningResource
            If MiningResource == Firewood01
                AddItem(MiningResource, 2, abSilent=True)
            Else
                AddItem(MiningResource, abSilent=True)
            EndIf
            UnregisterForUpdateGameTime()
            RegisterForSingleUpdateGameTime(0.056)
        EndIf
    EndIf

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

    UnregisterForUpdateGameTime()
    RegisterForSingleUpdateGameTime(0.056)

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

Function Aim(ObjectReference target, ObjectReference loc, Bool periodicAttack=False, Bool bypassLock=False)
    AcquireLock(bypassLock)

    If loc
        SetLinkedRef(BRSS_PackageKeyword1, loc)
    EndIf
    If target
        SetLinkedRef(BRSS_PackageKeyword2, target)
    EndIf
    If periodicAttack
        SetAV("Variable08", 10)
    Else
        SetAV("Variable08", 7)
    EndIf
    EvaluatePackage()

    If periodicAttack
        UnregisterForUpdate()
        RegisterForSingleUpdate(10.0)
    EndIf

    ReleaseLock(bypassLock)
EndFunction

Function Sit(ObjectReference target, Bool bypassLock=False)
    AcquireLock(bypassLock)

    SetLinkedRef(BRSS_PackageKeyword1, target)
    SetLinkedRef(BRSS_PackageKeyword2, None)
    SetAV("Variable08", 8)
    EvaluatePackage()

    UnregisterForUpdateGameTime()
    RegisterForSingleUpdateGameTime(0.056)

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

Function UseIdleMarkerWeaponDrawn(ObjectReference target, ObjectReference secondTarget=None, Bool bypassLock=False)
    AcquireLock(bypassLock)

    If target
        SetLinkedRef(BRSS_PackageKeyword1, target)
    EndIf
    If secondTarget
        SetLinkedRef(BRSS_PackageKeyword2, secondTarget)
    EndIf
    SetAV("Variable08", 10)
    EvaluatePackage()

    UnregisterForUpdate()
    RegisterForSingleUpdate(10.0)

    ReleaseLock(bypassLock)
EndFunction

Function Use(ObjectReference target, ObjectReference secondTarget=None, String miningRes="", Bool bypassLock=False)
    AcquireLock(bypassLock)

    If miningRes == "clay"
        MiningResource = BYOHMaterialClay
    ElseIf miningRes == "corundum"
        MiningResource = OreCorundum
    ElseIf miningRes == "ebony"
        MiningResource = OreEbony
    ElseIf miningRes == "gold"
        MiningResource = OreGold
    ElseIf miningRes == "iron"
        MiningResource = OreIron
    ElseIf miningRes == "malachite"
        MiningResource = OreMalachite
    ElseIf miningRes == "moonstone"
        MiningResource = OreMoonstone
    ElseIf miningRes == "orichalcum"
        MiningResource = OreOrichalcum
    ElseIf miningRes == "silver"
        MiningResource = OreSilver
    ElseIf miningRes == "stone"
        MiningResource = BYOHMaterialStoneBlock
    ElseIf miningRes == "quicksilver"
        MiningResource = OreQuicksilver
    ElseIf miningRes == "wood"
        MiningResource = Firewood01
    Else
        MiningResource = None
    EndIf

    If target.GetBaseObject() as Activator && !(target.GetBaseObject() as Furniture)
        target = target.GetLinkedRef()
    EndIf

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

Bool Function IsUsingIdleMarkerWeaponDrawn()
    Return GetAV("Variable08") as Int == 10
EndFunction

Function SetActorName(String value)
    If ! value
        Return
    EndIf

    Name = value
    SetDisplayName(value)
EndFunction

Function RemoveFromBRSS()
    RemoveFromFaction(BRSS_Actors)
    RemoveFromFaction(BRSS_Guards)
    RemoveFromFaction(BRSS_Slaves)
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
