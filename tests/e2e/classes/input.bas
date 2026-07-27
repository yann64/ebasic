TYPE Counter
    value AS INTEGER
    label AS STRING

    Declare Constructor()
    Declare Destructor()
    Declare Sub Increment()
    Declare Sub IncrementBy(amount AS INTEGER)
    Declare Function Get() AS INTEGER
    Declare Sub Bump()
    Declare Sub SetLabel(label AS STRING)
END TYPE

Constructor Counter()
    value = 0
    label = "counter"
    PRINT "created " & label
End Constructor

Destructor Counter()
    PRINT "destroyed " & label
End Destructor

Sub Counter.Increment()
    value = value + 1
End Sub

Sub Counter.IncrementBy(amount AS INTEGER)
    ' Parameter 'amount' doesn't shadow anything, but exercises a real
    ' parameter alongside implicit member access.
    value = value + amount
End Sub

Function Counter.Get() AS INTEGER
    Get = value
End Function

Sub Counter.Bump()
    ' One method calling another on itself.
    CALL This.Increment()
End Sub

Sub Counter.SetLabel(label AS STRING)
    ' Parameter 'label' shadows the member of the same name - This
    ' disambiguates.
    This.label = label
End Sub

DIM c AS Counter
PRINT c.Get()
CALL c.Increment()
PRINT c.Get()
CALL c.IncrementBy(10)
PRINT c.Get()
CALL c.Bump()
PRINT c.Get()
DIM newLabel AS STRING
newLabel = "renamed"
CALL c.SetLabel(newLabel)
PRINT c.label
