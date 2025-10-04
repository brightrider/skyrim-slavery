Scriptname BRSSControllerScript extends Quest

ActorBase Property BRSS_Guard Auto
ActorBase Property BRSS_Slave Auto

Function AddActor(String actorType, String name="")
    If actorType == "Guard"
        BRSSActorScript newActor = Game.GetPlayer().PlaceAtMe(BRSS_Guard) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
    ElseIf actorType == "Slave"
        BRSSActorScript newActor = Game.GetPlayer().PlaceAtMe(BRSS_Slave) as BRSSActorScript
        If name != ""
            newActor.SetActorName(name)
        EndIf
    EndIf
EndFunction
