Scriptname BRSSUtil

String Function GetFormID(ObjectReference ref) global
    Return StringUtil.Substring(PO3_SKSEFunctions.IntToString(ref.GetFormID(), abHex=True), 2)
EndFunction
