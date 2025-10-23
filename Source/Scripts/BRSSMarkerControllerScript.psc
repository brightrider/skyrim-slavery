Scriptname BRSSMarkerControllerScript extends Quest

Form Property EmptyIdleMarker Auto

Bool Lock = False

Function Add(String name)
    AcquireLock()

    ObjectReference marker = Game.GetPlayer().PlaceAtMe(EmptyIdleMarker)

    StorageUtil.StringListAdd(None, "BRSS_MarkerNames", name, allowDuplicate=False)
    StorageUtil.FormListAdd(None, "BRSS_Markers", marker, allowDuplicate=False)

    ReleaseLock()
EndFunction

ObjectReference Function Get(String name)
    AcquireLock()

    Int idx = StorageUtil.StringListFind(None, "BRSS_MarkerNames", name)
    ObjectReference marker = StorageUtil.FormListGet(None, "BRSS_Markers", idx) as ObjectReference

    ReleaseLock()
    Return marker
EndFunction

Function Remove(String name)
    AcquireLock()

    Int idx = StorageUtil.StringListFind(None, "BRSS_MarkerNames", name)
    ObjectReference marker = StorageUtil.FormListGet(None, "BRSS_Markers", idx) as ObjectReference

    StorageUtil.StringListRemoveAt(None, "BRSS_MarkerNames", idx)
    StorageUtil.FormListRemoveAt(None, "BRSS_Markers", idx)

    marker.Delete()

    ReleaseLock()
EndFunction

String[] Function GetNames()
    AcquireLock()

    String[] names = StorageUtil.StringListToArray(None, "BRSS_MarkerNames")

    ReleaseLock()
    Return names
EndFunction

; ##############################################################################
; # Private Functions
; ##############################################################################
Function AcquireLock(Bool bypass=False, Float spinDelay=0.001)
    If bypass
        Return
    EndIf

    While Lock
        Utility.Wait(spinDelay)
    EndWhile
    Lock = True
EndFunction

Function ReleaseLock(Bool bypass=False)
    If bypass
        Return
    EndIf

    Lock = False
EndFunction
; ##############################################################################
