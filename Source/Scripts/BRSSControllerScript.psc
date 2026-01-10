Scriptname BRSSControllerScript extends Quest

ActorBase Property BRSS_Guard Auto
ActorBase Property BRSS_Slave Auto
LeveledActor Property BRSS_LC Auto
LeveledActor Property BRSS_LC_Breton_Female Auto
LeveledActor Property BRSS_LC_Breton_Female_Child Auto
LeveledActor Property BRSS_LC_Breton_Female_Vampire Auto
LeveledActor Property BRSS_LC_Breton_Female_Vampire_Child Auto
LeveledActor Property BRSS_LC_DarkElf_Female Auto
LeveledActor Property BRSS_LC_DarkElf_Female_Vampire Auto
LeveledActor Property BRSS_LC_HighElf_Female Auto
LeveledActor Property BRSS_LC_HighElf_Female_Vampire Auto
LeveledActor Property BRSS_LC_Imperial_Female Auto
LeveledActor Property BRSS_LC_Imperial_Female_Child Auto
LeveledActor Property BRSS_LC_Imperial_Female_Vampire Auto
LeveledActor Property BRSS_LC_Nord_Female Auto
LeveledActor Property BRSS_LC_Nord_Female_Child Auto
LeveledActor Property BRSS_LC_Nord_Female_Vampire Auto
LeveledActor Property BRSS_LC_Redguard_Female Auto
LeveledActor Property BRSS_LC_Redguard_Female_Child Auto
LeveledActor Property BRSS_LC_WoodElf_Female Auto

Bool Lock = False

BRSSActorScript Function AddActor(String actorType, String name="", String actorRace="", Bool isVampire=False)
    AcquireLock()

    BRSSActorScript newActor

    If ! actorRace || actorRace == "random"
        Int idx = Utility.RandomInt(0, 6)
        If idx == 0
            actorRace = "nord"
        ElseIf idx == 1
            actorRace = "breton"
        ElseIf idx == 2
            actorRace = "darkelf"
        ElseIf idx == 3
            actorRace = "highelf"
        ElseIf idx == 4
            actorRace = "imperial"
        ElseIf idx == 5
            actorRace = "redguard"
        ElseIf idx == 6
            actorRace = "woodelf"
        EndIf
    EndIf

    If actorType == "Guard"
        SelectNpcTemplateList(actorRace, isVampire, False)
        newActor = Game.GetPlayer().PlaceAtMe(BRSS_Guard, abForcePersist=True) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
    ElseIf actorType == "Slave"
        SelectNpcTemplateList(actorRace, isVampire, False)
        newActor = Game.GetPlayer().PlaceAtMe(BRSS_Slave, abForcePersist=True) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
    ElseIf actorType == "Child"
        SelectNpcTemplateList(actorRace, isVampire, True)
        newActor = Game.GetPlayer().PlaceAtMe(BRSS_Slave, abForcePersist=True) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
        newActor.SetOutfit(Game.GetFormFromFile(0x03C81B, "Dragonborn.esm") as Outfit)
    EndIf

    ReleaseLock()
    Return newActor as BRSSActorScript
EndFunction

Function CreateConvoy(Form[] members)
    Int i = 1
    While i < members.Length
        (members[i] as BRSSActorScript).Follow(members[i - 1] as ObjectReference)
        i += 1
    EndWhile
EndFunction

Function TrackOnMap(Int slot, ObjectReference target)
    (GetAliasById(slot + 2) as ReferenceAlias).ForceRefTo(target)
    SetObjectiveDisplayed(slot, abForce=True)
EndFunction

Function UntrackOnMap(Int slot)
    SetObjectiveDisplayed(slot, False, abForce=True)
    (GetAliasById(slot + 2) as ReferenceAlias).Clear()
EndFunction

Function ExecutionSetup(Form[] guards, Form[] slaves, String markerGridName)
    BRSSMarkerControllerScript markerController = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript

    Int i = 0
    While i < guards.Length
        BRSSActorScript guard = guards[i] as BRSSActorScript
        BRSSActorScript slave = slaves[i] as BRSSActorScript
        guard.Use(markerController.Get(markerGridName + i), slave)
        slave.Use(markerController.Get(markerGridName + (guards.Length + i)))
        i += 1
    EndWhile
EndFunction

Function ExecutionIdle()
    ObjectReference[] actors = PO3_SKSEFunctions.FindAllReferencesOfFormType(Game.GetPlayer(), 43, 1024)
    Int i = 0
    While i < actors.Length
        BRSSActorScript npc = actors[i] as BRSSActorScript
        If npc.IsGuard() && (npc.IsUsingIdleMarker() || npc.IsAiming() || npc.IsUsingWeapon() || npc.IsUsingWeaponOnce())
            npc.UseIdleMarker(None, None)
        EndIf
        i += 1
    EndWhile
EndFunction

Function ExecutionAim()
    ObjectReference[] actors = PO3_SKSEFunctions.FindAllReferencesOfFormType(Game.GetPlayer(), 43, 1024)
    Int i = 0
    While i < actors.Length
        BRSSActorScript npc = actors[i] as BRSSActorScript
        If npc.IsGuard() && (npc.IsUsingIdleMarker() || npc.IsAiming() || npc.IsUsingWeapon() || npc.IsUsingWeaponOnce())
            npc.Aim(None, None)
        EndIf
        i += 1
    EndWhile
EndFunction

Function ExecutionFire()
    ObjectReference[] actors = PO3_SKSEFunctions.FindAllReferencesOfFormType(Game.GetPlayer(), 43, 1024)
    Int i = 0
    While i < actors.Length
        BRSSActorScript npc = actors[i] as BRSSActorScript
        If npc.IsGuard() && (npc.IsUsingIdleMarker() || npc.IsAiming() || npc.IsUsingWeapon() || npc.IsUsingWeaponOnce())
            npc.UseWeapon(None, None)
        EndIf
        i += 1
    EndWhile
EndFunction

Function ExecutionFireOnce()
    ObjectReference[] actors = PO3_SKSEFunctions.FindAllReferencesOfFormType(Game.GetPlayer(), 43, 1024)
    Int i = 0
    While i < actors.Length
        BRSSActorScript npc = actors[i] as BRSSActorScript
        If npc.IsGuard() && (npc.IsUsingIdleMarker() || npc.IsAiming() || npc.IsUsingWeapon() || npc.IsUsingWeaponOnce())
            npc.UseWeaponOnce(None, None)
        EndIf
        i += 1
    EndWhile
EndFunction

; ##############################################################################
; # Private Functions
; ##############################################################################
Function SelectNpcTemplateList(String type, Bool isVampire, Bool isChild)
    BRSS_LC.Revert()

    If type == "breton" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Breton_Female, 1)
    ElseIf type == "breton" && !isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Breton_Female_Child, 1)
    ElseIf type == "breton" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Breton_Female_Vampire, 1)
    ElseIf type == "breton" && isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Breton_Female_Vampire_Child, 1)
    ElseIf type == "darkelf" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_DarkElf_Female, 1)
    ElseIf type == "darkelf" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_DarkElf_Female_Vampire, 1)
    ElseIf type == "highelf" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_HighElf_Female, 1)
    ElseIf type == "highelf" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_HighElf_Female_Vampire, 1)
    ElseIf type == "imperial" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Imperial_Female, 1)
    ElseIf type == "imperial" && !isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Imperial_Female_Child, 1)
    ElseIf type == "imperial" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Imperial_Female_Vampire, 1)
    ElseIf type == "nord" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Nord_Female, 1)
    ElseIf type == "nord" && !isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Nord_Female_Child, 1)
    ElseIf type == "nord" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Nord_Female_Vampire, 1)
    ElseIf type == "redguard" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Redguard_Female, 1)
    ElseIf type == "redguard" && !isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Redguard_Female_Child, 1)
    ElseIf type == "woodelf" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_WoodElf_Female, 1)
    EndIf
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
