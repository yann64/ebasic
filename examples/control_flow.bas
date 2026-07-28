' See docs/reference/control-flow.md

DIM n AS INTEGER
n = 7
IF n > 10 THEN
    PRINT "big"
ELSEIF n > 5 THEN
    PRINT "medium"
ELSE
    PRINT "small"
END IF

SELECT CASE n
CASE 1, 2, 3
    PRINT "low"
CASE 7
    PRINT "seven"
CASE ELSE
    PRINT "other"
END SELECT

DIM i AS INTEGER
DIM total AS INTEGER
FOR i = 1 TO 10 STEP 2
    total = total + i
NEXT i
PRINT total

i = 0
DO WHILE i < 3
    PRINT i
    i = i + 1
LOOP

GOTO skip
PRINT "should not print"
skip:
PRINT "after goto"
