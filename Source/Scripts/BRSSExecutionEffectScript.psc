Scriptname BRSSExecutionEffectScript extends activemagiceffect

Event OnEffectStart(Actor akTarget, Actor akCaster)
    ObjectReference[] actors = PO3_SKSEFunctions.FindAllReferencesOfFormType(Game.GetPlayer(), 43, 1024)
    Int i = 0
    While i < actors.Length
        (actors[i] as BRSSActorScript).ToggleUseWeapon()
        i += 1
    EndWhile
EndEvent
