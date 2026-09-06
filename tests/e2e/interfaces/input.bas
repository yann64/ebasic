' M11: multiple-interface implementation - a TYPE may EXTENDS at most one
' ordinary (fielded) base plus any number of pure interfaces (a TYPE with
' zero fields where every method is Virtual).

TYPE IClickable
    Declare Virtual Sub OnClick()
END TYPE

TYPE IResizable
    Declare Virtual Sub OnResize(w AS INTEGER, h AS INTEGER)
END TYPE

TYPE BaseWidget
    label AS STRING
END TYPE

TYPE Widget EXTENDS BaseWidget, IClickable, IResizable
    clicks AS INTEGER

    Declare Virtual Sub OnClick()
    Declare Virtual Sub OnResize(w AS INTEGER, h AS INTEGER)
END TYPE

Virtual Sub Widget.OnClick()
    This.clicks = This.clicks + 1
    PRINT "clicked:"
    PRINT This.clicks
End Sub

Virtual Sub Widget.OnResize(w AS INTEGER, h AS INTEGER)
    PRINT "resized to"
    PRINT w
    PRINT h
End Sub

' Real dynamic dispatch through a BYREF interface-typed parameter -
' resolves to whichever concrete TYPE was actually passed, the
' polymorphism an interface exists for (matches the existing BYREF-Base-
' parameter precedent for ordinary single inheritance).
SUB PokeClickable(c AS IClickable)
    CALL c.OnClick()
END SUB

DIM w AS Widget
w.label = "Button"
CALL w.OnClick()
CALL w.OnResize(100, 200)
CALL PokeClickable(w)
PRINT w.clicks
