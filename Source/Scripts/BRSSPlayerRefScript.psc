Scriptname BRSSPlayerRefScript extends ReferenceAlias

Event OnInit()
    Init()
EndEvent

Event OnPlayerLoadGame()
    Init()
EndEvent

Function Init()
    SendModEvent("BRSSGameLoaded")
EndFunction
