Scriptname BRSSConsoleApi

String Function Test() global
    Return "BRSS: OK"
EndFunction

String Function ActorAdd(String actorType, String name) global
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript
    controller.AddActor(actorType, name)
    Return ""
EndFunction
