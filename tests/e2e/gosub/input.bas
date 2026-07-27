DIM counter AS INTEGER
counter = 0

' Called from two different call sites - each must return to its own place
PRINT "before first call"
GOSUB Bump
PRINT "after first call, counter ="
PRINT counter

PRINT "before second call"
GOSUB Bump
PRINT "after second call, counter ="
PRINT counter

GOTO SkipSub

Bump:
    counter = counter + 1
    RETURN

SkipSub:
PRINT "after skip"

' Nested GOSUB: one gosub target calls another
DIM total AS INTEGER
total = 0

GOSUB Outer
PRINT total

GOTO SkipNested

Outer:
    total = total + 1
    GOSUB Inner
    RETURN

Inner:
    total = total + 10
    RETURN

SkipNested:
PRINT "done"
