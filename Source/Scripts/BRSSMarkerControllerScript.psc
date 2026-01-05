Scriptname BRSSMarkerControllerScript extends Quest

Form Property EmptyIdleMarker Auto

Bool Lock = False

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

    StorageUtil.StringListAdd(None, "BRSS_MarkerNames", name, allowDuplicate=False)
    StorageUtil.FormListAdd(None, "BRSS_Markers", marker, allowDuplicate=False)

    ReleaseLock()
    Return marker
EndFunction

ObjectReference Function Get(String name, Bool bypassLock=False)
    AcquireLock(bypassLock)

    Int idx = StorageUtil.StringListFind(None, "BRSS_MarkerNames", name)
    ObjectReference marker = StorageUtil.FormListGet(None, "BRSS_Markers", idx) as ObjectReference

    ReleaseLock(bypassLock)
    Return marker
EndFunction

Function Rename(String oldName, String newName)
    If ! newName
        Return
    EndIf

    AcquireLock()

    Int idx = StorageUtil.StringListFind(None, "BRSS_MarkerNames", oldName)
    If idx >= 0
        StorageUtil.StringListSet(None, "BRSS_MarkerNames", idx, newName)
    EndIf

    ReleaseLock()
EndFunction

Bool Function Remove(String name)
    AcquireLock()

    Int idx = StorageUtil.StringListFind(None, "BRSS_MarkerNames", name)
    If idx == -1
        ReleaseLock()
        Return False
    EndIf
    ObjectReference marker = StorageUtil.FormListGet(None, "BRSS_Markers", idx) as ObjectReference

    StorageUtil.StringListRemoveAt(None, "BRSS_MarkerNames", idx)
    StorageUtil.FormListRemoveAt(None, "BRSS_Markers", idx)

    marker.Delete()

    ReleaseLock()
    Return True
EndFunction

String Function GetList(String filter="", Float radius=0.0)
    String result

    AcquireLock()

    Int i
    While i < StorageUtil.StringListCount(None, "BRSS_MarkerNames")
        String name = StorageUtil.StringListGet(None, "BRSS_MarkerNames", i)
        ObjectReference marker = StorageUtil.FormListGet(None, "BRSS_Markers", i) as ObjectReference

        If radius <= 0.0 || marker.GetDistance(Game.GetPlayer()) <= radius
            If !filter || StringUtil.Find(name, filter) != -1
                result += name + " " + "(" + marker.GetCurrentLocation().GetName() + ")" + "\n"
            EndIf
        EndIf

        i += 1
    EndWhile

    ReleaseLock()
    Return result
EndFunction

Function CreateGrid(String name, Int[] grid, Int width, Form markerForm=None, Int offX=128, Int offY=256)
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
