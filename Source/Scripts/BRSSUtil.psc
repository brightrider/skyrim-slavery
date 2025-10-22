Scriptname BRSSUtil

String Function GetFormID(ObjectReference ref) global
    Return StringUtil.Substring(PO3_SKSEFunctions.IntToString(ref.GetFormID(), abHex=True), 2)
EndFunction

ObjectReference Function GetRuntimeRef(Int id) global
    Return Game.GetFormEx(Math.LogicalOr(id, 0xFF000000)) as ObjectReference
EndFunction
