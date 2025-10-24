Scriptname BRSSMath

; ##############################################################################
; # Geometry transformations
; ##############################################################################
Function TranslateLocalXY(ObjectReference obj, Float x, Float y) global
    Float newX = obj.X + (Math.Sin(obj.GetAngleZ() + 90.0) * x)
    Float newY = obj.Y + (Math.Cos(obj.GetAngleZ() + 90.0) * x)
    newX += Math.Sin(obj.GetAngleZ()) * y
    newY += Math.Cos(obj.GetAngleZ()) * y
    obj.SetPosition(newX, newY, obj.Z)
EndFunction

Function RotateZ(ObjectReference obj, Float angle) global
    Float newAngle = obj.GetAngleZ() + angle
    If newAngle >= 360.0
        newAngle -= 360.0
    ElseIf newAngle < 0.0
        newAngle += 360.0
    EndIf
    obj.SetAngle(obj.X, obj.Y, newAngle)
EndFunction
; ##############################################################################
