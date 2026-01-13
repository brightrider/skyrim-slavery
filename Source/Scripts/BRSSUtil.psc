Scriptname BRSSUtil

Actor[] Function GetAllActors() global
    Return PO3_SKSEFunctions.GetAllActorsInFaction(Game.GetFormFromFile(0x00005900, "SkyrimSlavery.esp") as Faction)
EndFunction

Actor Function GetActorByDisplayName(String name) global
    If ! name
        Return None
    EndIf

    Actor[] actors = GetAllActors()
    Int i = 0
    While i < actors.Length
        If actors[i].GetDisplayName() == name
            Return actors[i]
        EndIf
        i += 1
    EndWhile
EndFunction

Actor Function GetPlayerOrActorByDisplayName(String name) global
    If name == "player"
        Return Game.GetPlayer()
    EndIf
    Return GetActorByDisplayName(name)
EndFunction

ObjectReference Function GetActorOrMarkerByDisplayName(String name) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript

    ObjectReference result = controller.Get(name)
    If result
        Return result
    EndIf

    Return GetPlayerOrActorByDisplayName(name)
EndFunction

String Function GetFormID(ObjectReference ref) global
    Return StringUtil.Substring(PO3_SKSEFunctions.IntToString(ref.GetFormID(), abHex=True), 2)
EndFunction

String Function GetName(ObjectReference ref, Bool skipMarkerName=False) global
    BRSSMarkerControllerScript controller = Game.GetFormFromFile(0x00047627, "SkyrimSlavery.esp") as BRSSMarkerControllerScript

    String result = ref.GetDisplayName()
    If result && StringUtil.Find(result, "not be visible") == -1
        Return result
    EndIf

    result = ref.GetBaseObject().GetName()
    If result && StringUtil.Find(result, "not be visible") == -1
        Return result
    EndIf

    If ! skipMarkerName
        result = controller.GetMarkerName(ref)
        If result
            Return result
        EndIf
    EndIf

    Return PO3_SKSEFunctions.GetFormEditorID(ref.GetBaseObject())
EndFunction

Form Function GetForm(String id) global
    Int numId = PO3_SKSEFunctions.StringToInt(id)
    If numId == 0 || numId == -1
        numId = PO3_SKSEFunctions.StringToInt("0x" + id)
    EndIf

    If numId != 0 && numId != -1
        Return Game.GetFormEx(numId)
    EndIf
    Return PO3_SKSEFunctions.GetFormFromEditorID(id)
EndFunction

ObjectReference Function GetRef(String id) global
    If ! id || id == "s" || id == "selected"
        Return Game.GetCurrentCrosshairRef()
    EndIf

    Int numId = PO3_SKSEFunctions.StringToInt(id)
    If numId == 0 || numId == -1
        numId = PO3_SKSEFunctions.StringToInt("0x" + id)
    EndIf
    If numId != 0 && numId != -1
        Return Game.GetFormEx(numId) as ObjectReference
    EndIf

    Return GetActorOrMarkerByDisplayName(id)
EndFunction

Form[] Function DisplayNamesToRefArray(String arg, String sep=",") global
    String[] parts = StringUtil.Split(arg, sep)
    Form[] result = Utility.CreateFormArray(parts.Length)
    Int i = 0
    While i < parts.Length
        result[i] = GetPlayerOrActorByDisplayName(parts[i])
        i += 1
    EndWhile

    Return result
EndFunction
