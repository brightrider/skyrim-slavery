Scriptname BRSSConsoleApi

String Function Test() global
    Return "BRSS: OK"
EndFunction

String Function ActorAdd(String actorType, String name, String actorRace, Bool isVampire) global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.AddActor(actorType, name, actorRace, isVampire)
    Return ""
EndFunction

String Function ActorList(String filter, Float radius) global
    String result = ""

    Actor[] actors = BRSSUtil.GetActors(Game.GetFormFromFile(0x5900, "SkyrimSlavery.esp") as Faction, afRadius=radius)
    Int i = 0
    While i < actors.Length
        String line = (actors[i] as BRSSActorScript).GetDescription() + "\n"
        If !filter || StringUtil.Find(line, filter) != -1
            result += line
        EndIf

        i += 1
    EndWhile

    Return result
EndFunction

String Function ActorRename(Actor actorId, String name) global
    (actorId as BRSSActorScript).SetActorName(name)
    Return ""
EndFunction

String Function ActorDel(Actor actorId) global
    (actorId as BRSSActorScript).RemoveFromBRSS()
    Return ""
EndFunction

String Function ActorSetOutfit(Actor actorId, Outfit outfitForm) global
    (actorId as BRSSActorScript).SetOutfit(outfitForm)
    Return ""
EndFunction

String Function ActorWait(Actor actorId) global
    (actorId as BRSSActorScript).Wait()
    Return ""
EndFunction

String Function ActorTravel(Actor actorId, String markerName) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    (actorId as BRSSActorScript).Travel(controller.Get(markerName))
    Return ""
EndFunction

String Function ActorFollow(String actorList) global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.CreateConvoy(BRSSUtil.DisplayNamesToRefArray(actorList))
    Return ""
EndFunction

String Function ActorUse(Actor actorId, String targetId, String targetActorName) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    (actorId as BRSSActorScript).Use(BRSSUtil.GetRef(targetId), BRSSUtil.GetActorByDisplayName(targetActorName))
    Return ""
EndFunction

String Function ActorPatrol(Actor actorId, String point1, String point2) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    (actorId as BRSSActorScript).Patrol(controller.Get(point1), controller.Get(point2))
    Return ""
EndFunction

String Function ActorAim(Actor actorId, String name) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    (actorId as BRSSActorScript).Aim(BRSSUtil.GetActorByDisplayName(name), None)
    Return ""
EndFunction

String Function ActorUseWeapon(Actor actorId, String name) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    (actorId as BRSSActorScript).UseWeapon(BRSSUtil.GetActorByDisplayName(name), None)
    Return ""
EndFunction

String Function ActorUseWeaponOnce(Actor actorId, String name) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    (actorId as BRSSActorScript).UseWeaponOnce(BRSSUtil.GetActorByDisplayName(name), None)
    Return ""
EndFunction

String Function MarkerAdd(String name, String markerForm) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    controller.Add(name, BRSSUtil.GetForm(markerForm))
    Return ""
EndFunction

String Function MarkerList(String filter, Float radius) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    Return controller.GetList(filter, radius)
EndFunction

String Function MarkerRename(String oldName, String newName) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    controller.Rename(oldName, newName)
    Return ""
EndFunction

String Function MarkerDel(String name) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    controller.Remove(name)
    Return ""
EndFunction

String Function MarkerGridAdd(String name, String grid, Int width, String markerForm, Int offX, Int offY) global
    String[] tokens = StringUtil.Split(grid, ",")
    Int[] gridData = Utility.CreateIntArray(tokens.Length)
    Int i = 0
    While i < tokens.Length
        gridData[i] = PO3_SKSEFunctions.StringToInt(tokens[i])
        i += 1
    EndWhile

    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    controller.CreateGrid(name, gridData, width, BRSSUtil.GetForm(markerForm), offX, offY)

    Return ""
EndFunction

String Function MarkerGridDel(String name) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    controller.DestroyGrid(name)
    Return ""
EndFunction

String Function TrackingTrack(Int slot, String targetId) global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.TrackOnMap(slot, BRSSUtil.GetRef(targetId))
    Return ""
EndFunction

String Function TrackingList() global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript

    String result
    Int i
    While i < 10
        ObjectReference ref = (controller.GetAliasById(i + 2) as ReferenceAlias).GetReference()
        result += "Slot " + i + ": " + BRSSUtil.GetName(ref) + "\n"
        i += 1
    EndWhile

    Return result
EndFunction

String Function TrackingUntrack(Int slot) global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.UntrackOnMap(slot)
    Return ""
EndFunction

String Function ExecutionSetup(String guardList, String slaveList, String markerGridName) global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.ExecutionSetup(BRSSUtil.DisplayNamesToRefArray(guardList), BRSSUtil.DisplayNamesToRefArray(slaveList), markerGridName)
    Return ""
EndFunction

String Function ExecutionIdle() global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.ExecutionIdle()
    Return ""
EndFunction

String Function ExecutionAim() global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.ExecutionAim()
    Return ""
EndFunction

String Function ExecutionFire() global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.ExecutionFire()
    Return ""
EndFunction

String Function ExecutionFireOnce() global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.ExecutionFireOnce()
    Return ""
EndFunction
