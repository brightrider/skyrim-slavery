Scriptname BRSSConsoleApi

String Function Test() global
    Return "BRSS: OK"
EndFunction

String Function ActorAdd(String actorType, String name) global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.AddActor(actorType, name)
    Return ""
EndFunction

String Function ActorList(String filter, Float radius) global
    String result = ""

    Keyword actorKwd = Game.GetFormFromFile(0x00042524, "SkyrimSlavery.esp") as Keyword
    Faction factionAll = Game.GetFormFromFile(0x00005900, "SkyrimSlavery.esp") as Faction

    Actor[] actors
    ObjectReference[] actorsAsRefs
    If radius > 0.0
        actorsAsRefs = PO3_SKSEFunctions.FindAllReferencesWithKeyword(Game.GetPlayer(), actorKwd, radius, abMatchAll=False)
    Else
        actors = PO3_SKSEFunctions.GetAllActorsInFaction(factionAll)
    EndIf

    Int i = 0
    While i < actors.Length || i < actorsAsRefs.Length
        BRSSActorScript act = actors[i] as BRSSActorScript
        If act == None
            act = actorsAsRefs[i] as BRSSActorScript
        EndIf

        String line = act.GetDescription() + "\n"

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

    ObjectReference target
    If StringUtil.GetNthChar(targetId, 0) == "0" && StringUtil.GetNthChar(targetId, 1) == "x"
        target = Game.GetFormEx(PO3_SKSEFunctions.StringToInt(targetId)) as ObjectReference
    Else
        target = controller.Get(targetId)
    EndIf

    (actorId as BRSSActorScript).Use(target, BRSSUtil.GetActorByDisplayName(targetActorName))
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

String Function MarkerAdd(String name, Form markerForm) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    controller.Add(name, markerForm)
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

String Function MarkerGridAdd(String name, String grid, Int width, Form markerForm, Int offX, Int offY) global
    String[] tokens = StringUtil.Split(grid, ",")
    Int[] gridData = Utility.CreateIntArray(tokens.Length)
    Int i = 0
    While i < tokens.Length
        gridData[i] = PO3_SKSEFunctions.StringToInt(tokens[i])
        i += 1
    EndWhile

    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    controller.CreateGrid(name, gridData, width, markerForm, offX, offY)

    Return ""
EndFunction

String Function MarkerGridDel(String name) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript
    controller.DestroyGrid(name)
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
