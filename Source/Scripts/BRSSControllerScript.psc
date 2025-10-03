Scriptname BRSSControllerScript extends Quest

ActorBase Property BRSS_Guard Auto
ActorBase Property BRSS_Slave Auto

Function AddActor(String actorType, String name="")
    If actorType == "Guard"
        Actor newActor = Game.GetPlayer().PlaceAtMe(BRSS_Guard) as Actor
        If name != ""
            newActor.SetDisplayName(name)
        EndIf
    ElseIf actorType == "Slave"
        Actor newActor = Game.GetPlayer().PlaceAtMe(BRSS_Slave) as Actor
        If name != ""
            newActor.SetDisplayName(name)
        EndIf
    EndIf
EndFunction
