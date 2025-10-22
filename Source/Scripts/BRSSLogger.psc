Scriptname BRSSLogger

Function LogInfo(String moduleName, ObjectReference ref, String msg) global
    Debug.Trace("[" + moduleName + ", " + GetLogID(ref) + ", INF] " + msg, 0)
EndFunction

Function LogWarning(String moduleName, ObjectReference ref, String msg) global
    Debug.Trace("[" + moduleName + ", " + GetLogID(ref) + ", WRN] " + msg, 1)
EndFunction

Function LogError(String moduleName, ObjectReference ref, String msg) global
    Debug.Trace("[" + moduleName + ", " + GetLogID(ref) + ", ERR] " + msg, 2)
EndFunction

String Function GetLogID(ObjectReference ref) global
    String name = ref.GetDisplayName()
    If name == ""
        name = PO3_SKSEFunctions.GetFormEditorID(ref)
    EndIf

    Return BRSSUtil.GetFormID(ref) + ":" + name
EndFunction
