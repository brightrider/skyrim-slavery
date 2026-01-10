Scriptname BRSSUtil

Actor[] Function GetAllActors() global
    Return PO3_SKSEFunctions.GetAllActorsInFaction(Game.GetFormFromFile(0x00005900, "SkyrimSlavery.esp") as Faction)
EndFunction

Actor Function GetActorByDisplayName(String name) global
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

String Function GetFormID(ObjectReference ref) global
    Return StringUtil.Substring(PO3_SKSEFunctions.IntToString(ref.GetFormID(), abHex=True), 2)
EndFunction

Form Function GetForm(String id) global
    Int numId = PO3_SKSEFunctions.StringToInt(id)
    If ! numId
        numId = PO3_SKSEFunctions.StringToInt("0x" + id)
    EndIf

    If numId
        Return Game.GetFormEx(numId)
    EndIf
    Return PO3_SKSEFunctions.GetFormFromEditorID(id)
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
