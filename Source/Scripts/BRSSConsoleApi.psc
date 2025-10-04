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
