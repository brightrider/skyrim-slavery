Scriptname BRSSLogger

Function LogInfo(String moduleName, ObjectReference ref, String msg) global
    Debug.Trace("[" + moduleName + ", " + ref.GetDisplayName() + ", INF] " + msg, 0)
EndFunction

Function LogWarning(String moduleName, ObjectReference ref, String msg) global
    Debug.Trace("[" + moduleName + ", " + ref.GetDisplayName() + ", WRN] " + msg, 1)
EndFunction

Function LogError(String moduleName, ObjectReference ref, String msg) global
    Debug.Trace("[" + moduleName + ", " + ref.GetDisplayName() + ", ERR] " + msg, 2)
EndFunction
