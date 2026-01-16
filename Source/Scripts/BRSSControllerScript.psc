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

Faction Property CWPlayerAlly Auto

Message Property BRSS_SelectNpcType Auto
Message Property BRSS_SelectRace Auto

Bool Lock = False

BRSSActorScript Function AddActor(String actorType, String name="", String actorRace="", Bool isVampire=False)
    AcquireLock()

    BRSSActorScript newActor

    If ! actorRace || actorRace == "random"
        If actorType != "Child" && isVampire
            actorRace = GetRaceByIdx(Utility.RandomInt(0, 4))
        ElseIf actorType == "Child" && !isVampire
            actorRace = GetRaceByIdxChild(Utility.RandomInt(0, 3))
        ElseIf actorType == "Child" && isVampire
            actorRace = "breton"
        Else
            actorRace = GetRaceByIdx(Utility.RandomInt(0, 6))
        EndIf
    EndIf

    If actorType == "Guard"
        If ! SelectNpcTemplateList(actorRace, isVampire, False)
            ReleaseLock()
            Return None
        EndIf
        newActor = Game.GetPlayer().PlaceAtMe(BRSS_Guard, abForcePersist=True) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
    ElseIf actorType == "Slave"
        If ! SelectNpcTemplateList(actorRace, isVampire, False)
            ReleaseLock()
            Return None
        EndIf
        newActor = Game.GetPlayer().PlaceAtMe(BRSS_Slave, abForcePersist=True) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
    ElseIf actorType == "Child"
        If ! SelectNpcTemplateList(actorRace, isVampire, True)
            ReleaseLock()
            Return None
        EndIf
        newActor = Game.GetPlayer().PlaceAtMe(BRSS_Slave, abForcePersist=True) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
        newActor.SetOutfit(Game.GetFormFromFile(0x835, "SkyChild.esp") as Outfit) ; 5 - d
    EndIf

    ReleaseLock()
    Return newActor
EndFunction

Function BuyActor()
    Actor player = Game.GetPlayer()
    Form gold = Game.GetForm(0xF)

    Int npcType = BRSS_SelectNpcType.Show()
    If npcType == 7
        Return
    EndIf

    String npcRace = GetRaceByIdx(BRSS_SelectRace.Show())
    If ! npcRace
        Return
    EndIf

    If npcType == 0
        If player.GetItemCount(gold) >= 500
            BRSSActorScript newActor = AddActor("Guard", "", npcRace, isVampire=False)
            newActor.AddToFaction(CWPlayerAlly)
            player.RemoveItem(gold, 500)
        Else
            Debug.Notification("You don't have enough gold.")
        EndIf
    ElseIf npcType == 1
        If player.GetItemCount(gold) >= 1500
            BRSSActorScript newActor = AddActor("Guard", "", npcRace, isVampire=True)
            If ! newActor
                Debug.Notification("This race is currently not available for vampires.")
                Return
            EndIf
            newActor.AddToFaction(CWPlayerAlly)
            player.RemoveItem(gold, 1500)
        Else
            Debug.Notification("You don't have enough gold.")
        EndIf
    ElseIf npcType == 2
        If player.GetItemCount(gold) >= 500
            BRSSActorScript newActor = AddActor("Slave", "", npcRace, isVampire=False)
            OBodyNative.ApplyPresetByName(newActor, "Lust")
            player.RemoveItem(gold, 500)
        Else
            Debug.Notification("You don't have enough gold.")
        EndIf
    ElseIf npcType == 3
        If player.GetItemCount(gold) >= 3000
            If ! AddActor("Slave", "", npcRace, isVampire=True)
                Debug.Notification("This race is currently not available for vampires.")
                Return
            EndIf
            player.RemoveItem(gold, 3000)
        Else
            Debug.Notification("You don't have enough gold.")
        EndIf
    ElseIf npcType == 4
        If player.GetItemCount(gold) >= 1000
            BRSSActorScript newActor = AddActor("Slave", "", npcRace, isVampire=False)
            OBodyNative.ApplyPresetByName(newActor, "Pregnant")
            player.RemoveItem(gold, 1000)
        Else
            Debug.Notification("You don't have enough gold.")
        EndIf
    ElseIf npcType == 5
        If player.GetItemCount(gold) >= 500
            If ! AddActor("Child", "", npcRace, isVampire=False)
                Debug.Notification("This race is currently not available for children.")
                Return
            EndIf
            player.RemoveItem(gold, 500)
        Else
            Debug.Notification("You don't have enough gold.")
        EndIf
    ElseIf npcType == 6
        If player.GetItemCount(gold) >= 3000
            If ! AddActor("Child", "", npcRace, isVampire=True)
                Debug.Notification("This race is currently not available for vampire children.")
                Return
            EndIf
            player.RemoveItem(gold, 3000)
        Else
            Debug.Notification("You don't have enough gold.")
        EndIf
    EndIf
EndFunction

Function CreateConvoy(Actor[] members)
    Int i = 1
    While i < members.Length
        (members[i] as BRSSActorScript).Follow(members[i - 1])
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

Function ExecutionSetup(Actor[] guards, Actor[] slaves, String markerGridName)
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
Bool Function SelectNpcTemplateList(String type, Bool isVampire, Bool isChild)
    BRSS_LC.Revert()

    If type == "breton" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Breton_Female, 1)
        Return True
    ElseIf type == "breton" && !isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Breton_Female_Child, 1)
        Return True
    ElseIf type == "breton" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Breton_Female_Vampire, 1)
        Return True
    ElseIf type == "breton" && isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Breton_Female_Vampire_Child, 1)
        Return True
    ElseIf type == "darkelf" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_DarkElf_Female, 1)
        Return True
    ElseIf type == "darkelf" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_DarkElf_Female_Vampire, 1)
        Return True
    ElseIf type == "highelf" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_HighElf_Female, 1)
        Return True
    ElseIf type == "highelf" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_HighElf_Female_Vampire, 1)
        Return True
    ElseIf type == "imperial" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Imperial_Female, 1)
        Return True
    ElseIf type == "imperial" && !isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Imperial_Female_Child, 1)
        Return True
    ElseIf type == "imperial" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Imperial_Female_Vampire, 1)
        Return True
    ElseIf type == "nord" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Nord_Female, 1)
        Return True
    ElseIf type == "nord" && !isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Nord_Female_Child, 1)
        Return True
    ElseIf type == "nord" && isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Nord_Female_Vampire, 1)
        Return True
    ElseIf type == "redguard" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_Redguard_Female, 1)
        Return True
    ElseIf type == "redguard" && !isVampire && isChild
        BRSS_LC.AddForm(BRSS_LC_Redguard_Female_Child, 1)
        Return True
    ElseIf type == "woodelf" && !isVampire && !isChild
        BRSS_LC.AddForm(BRSS_LC_WoodElf_Female, 1)
        Return True
    EndIf

    Return False
EndFunction

String Function GetRaceByIdx(Int idx)
    If idx == 0
        Return "nord"
    ElseIf idx == 1
        Return "breton"
    ElseIf idx == 2
        Return "darkelf"
    ElseIf idx == 3
        Return "highelf"
    ElseIf idx == 4
        Return "imperial"
    ElseIf idx == 5
        Return "redguard"
    ElseIf idx == 6
        Return "woodelf"
    EndIf
EndFunction

String Function GetRaceByIdxChild(Int idx)
    If idx == 0
        Return "nord"
    ElseIf idx == 1
        Return "breton"
    ElseIf idx == 2
        Return "imperial"
    ElseIf idx == 3
        Return "redguard"
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
