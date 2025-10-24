Scriptname BRSSMarkerControllerScript extends Quest

Form Property EmptyIdleMarker Auto

Bool Lock = False

ObjectReference Function Add(String name)
    AcquireLock()

    ObjectReference marker = Game.GetPlayer().PlaceAtMe(EmptyIdleMarker)

    StorageUtil.StringListAdd(None, "BRSS_MarkerNames", name, allowDuplicate=False)
    StorageUtil.FormListAdd(None, "BRSS_Markers", marker, allowDuplicate=False)

    ReleaseLock()
    Return marker
EndFunction

ObjectReference Function Get(String name)
    AcquireLock()

    Int idx = StorageUtil.StringListFind(None, "BRSS_MarkerNames", name)
    ObjectReference marker = StorageUtil.FormListGet(None, "BRSS_Markers", idx) as ObjectReference

    ReleaseLock()
    Return marker
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

String[] Function GetNames()
    AcquireLock()

    String[] names = StorageUtil.StringListToArray(None, "BRSS_MarkerNames")

    ReleaseLock()
    Return names
EndFunction

Function CreateGrid(String name, Int[] grid, Int width, Int offX=192, Int offY=512)
    Int i = 0
    While i < grid.Length
        ObjectReference marker = Add(name + i)

        Int newX = offX * (i % width)
        Int newY = offY * (i / width)
        marker.SetAngle(0.0, 0.0, marker.GetAngleZ())
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
