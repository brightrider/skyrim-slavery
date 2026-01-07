Scriptname BRSSControllerScript extends Quest

ActorBase Property BRSS_Guard Auto
ActorBase Property BRSS_Slave Auto

Function AddActor(String actorType, String name="")
    If actorType == "Guard"
        BRSSActorScript newActor = Game.GetPlayer().PlaceAtMe(BRSS_Guard, abForcePersist=True) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
    ElseIf actorType == "Slave"
        BRSSActorScript newActor = Game.GetPlayer().PlaceAtMe(BRSS_Slave, abForcePersist=True) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
    EndIf
EndFunction

Function CreateConvoy(Form[] members)
    Int i = 1
    While i < members.Length
        (members[i] as BRSSActorScript).Follow(members[i - 1] as ObjectReference)
        i += 1
    EndWhile
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
