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

    marker.Delete()

    ReleaseLock()
    Return True
EndFunction

String Function GetList(String filter="", Float radius=0.0)
    String result

    String _key = JMap.nextKey(MarkerNames)
    While _key
        String name = _key
        ObjectReference marker = JMap.getForm(MarkerNames, _key) as ObjectReference

        Float distance = marker.GetDistance(Game.GetPlayer())
        If radius <= 0.0 || distance <= radius
            If distance > 1000000.0 || distance < 0.0
                distance = -1.0
            EndIf

            name += "["
            name += BRSSUtil.GetFormID(marker) + ", "
            name += BRSSUtil.GetName(marker, skipMarkerName=True) + ", "
            name += marker.GetCurrentLocation().GetName() + "(" + distance + ")"
            name += "]"
            name += "\n"

            If !filter || StringUtil.Find(name, filter) != -1
                result += name
            EndIf
        EndIf

        _key = JMap.nextKey(MarkerNames, _key)
    EndWhile

    Return result
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
