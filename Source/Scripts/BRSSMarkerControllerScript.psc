Scriptname BRSSMarkerControllerScript extends Quest

Form Property EmptyIdleMarker Auto

Int Markers
Int MarkerNames

Bool Lock = False

Event OnInit()
    Markers = JFormMap.object()
    MarkerNames = JMap.object()
    JValue.retain(Markers)
    JValue.retain(MarkerNames)
EndEvent

ObjectReference Function Add(String name, Form markerForm=None)
    AcquireLock()

    If ! name || Get(name, bypassLock=True)
        ReleaseLock()
        Return None
    EndIf

    If ! markerForm
        markerForm = EmptyIdleMarker
    EndIf

    ObjectReference marker = Game.GetPlayer().PlaceAtMe(markerForm, abForcePersist=True)

    marker.SetAngle(0.0, 0.0, marker.GetAngleZ())

    JMap.setForm(MarkerNames, name, marker)
    JFormMap.setStr(Markers, marker, name)

    StorageUtil.StringListAdd(None, "BRSS_MarkerNames", name)
    StorageUtil.FormListAdd(None, "BRSS_Markers", marker)

    ReleaseLock()
    Return marker
EndFunction

ObjectReference Function Get(String name, Bool bypassLock=False)
    AcquireLock(bypassLock)

    ObjectReference marker = JMap.getForm(MarkerNames, name) as ObjectReference

    ReleaseLock(bypassLock)
    Return marker
EndFunction

String Function GetMarkerName(ObjectReference marker, Bool bypassLock=False)
    AcquireLock(bypassLock)

    String name = JFormMap.getStr(Markers, marker)

    ReleaseLock(bypassLock)
    Return name
EndFunction

Function Rename(String oldName, String newName)
    AcquireLock()

    If ! newName || Get(newName, bypassLock=True)
        ReleaseLock()
        Return
    EndIf

    ObjectReference marker = JMap.getForm(MarkerNames, oldName) as ObjectReference
    If marker
        JMap.removeKey(MarkerNames, oldName)
        JMap.setForm(MarkerNames, newName, marker)
        JFormMap.setStr(Markers, marker, newName)
    EndIf

    Int idx = StorageUtil.StringListFind(None, "BRSS_MarkerNames", oldName)
    If idx >= 0
        StorageUtil.StringListSet(None, "BRSS_MarkerNames", idx, newName)
    EndIf

    ReleaseLock()
EndFunction

Bool Function Remove(String name)
    AcquireLock()

    ObjectReference marker = JMap.getForm(MarkerNames, name) as ObjectReference
    If ! marker
        ReleaseLock()
        Return False
    EndIf
    JMap.removeKey(MarkerNames, name)
    JFormMap.removeKey(Markers, marker)

    Int idx = StorageUtil.StringListFind(None, "BRSS_MarkerNames", name)
    If idx >= 0
        StorageUtil.StringListRemoveAt(None, "BRSS_MarkerNames", idx)
        StorageUtil.FormListRemoveAt(None, "BRSS_Markers", idx)
    EndIf

    marker.Delete()

    ReleaseLock()
    Return True
EndFunction

String Function GetList(String filter="", Float radius=0.0)
    AcquireLock()

    BRSSUtil.LogMarkers(StorageUtil.FormListToArray(None, "BRSS_Markers"), StorageUtil.StringListToArray(None, "BRSS_MarkerNames"), filter, radius)

    ReleaseLock()
    Return ""
EndFunction

Function CreateGrid(String name, Int[] grid, Int width, Form markerForm=None, Int offX=128, Int offY=384)
    Int i = 0
    While i < grid.Length
        ObjectReference marker = Add(name + i, markerForm)

        Int newX = offX * (i % width)
        Int newY = offY * (i / width)
        BRSSMath.TranslateLocalXY(marker, newX, newY)
        BRSSMath.RotateZ(marker, grid[i])

        i += 1
    EndWhile
EndFunction

Function DestroyGrid(String name)
    Int i = 0
    While True
        If ! Remove(name + i)
            Return
        EndIf

        i += 1
    EndWhile
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
