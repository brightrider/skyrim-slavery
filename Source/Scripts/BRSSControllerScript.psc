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
