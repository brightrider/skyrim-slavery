Scriptname BRSSExecutionEffectScript extends activemagiceffect

Event OnEffectStart(Actor akTarget, Actor akCaster)
    BRSSControllerScript controller = Game.GetFormFromFile(0x0002E123, "SkyrimSlavery.esp") as BRSSControllerScript

    UIListMenu listMenu = UIExtensions.GetMenu("UIListMenu") As UIListMenu
    listMenu.AddEntryItem("Idle")
    listMenu.AddEntryItem("Aim")
    listMenu.AddEntryItem("Fire")
    listMenu.AddEntryItem("Fire Once")
    listMenu.OpenMenu()

    Int choice = listMenu.GetResultInt()
    If choice == 0
        controller.ExecutionIdle()
    ElseIf choice == 1
        controller.ExecutionAim()
    ElseIf choice == 2
        controller.ExecutionFire()
    ElseIf choice == 3
        controller.ExecutionFireOnce()
    EndIf
EndEvent
