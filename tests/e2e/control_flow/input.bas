DIM i AS INTEGER
DIM n AS INTEGER
DIM total AS INTEGER

' IF/ELSEIF/ELSE
n = 7
IF n > 10 THEN
    PRINT "big"
ELSEIF n > 5 THEN
    PRINT "medium"
ELSE
    PRINT "small"
END IF

' SELECT CASE
SELECT CASE n
CASE 1, 2, 3
    PRINT "low"
CASE 7
    PRINT "seven"
CASE ELSE
    PRINT "other"
END SELECT

' FOR/NEXT with STEP
total = 0
FOR i = 1 TO 10 STEP 2
    total = total + i
NEXT i
PRINT total

' FOR/NEXT counting down
FOR i = 5 TO 1 STEP -1
    PRINT i
NEXT i

' DO WHILE / LOOP
i = 0
DO WHILE i < 3
    PRINT "dowhile ", i
    i = i + 1
LOOP

' DO / LOOP UNTIL (post-test)
i = 0
DO
    PRINT "postuntil"
    i = i + 1
LOOP UNTIL i >= 2

' WHILE/WEND
i = 0
WHILE i < 3
    PRINT "wend ", i
    i = i + 1
WEND

' Nested EXIT FOR from inside a DO loop
FOR i = 1 TO 5
    DO
        IF i = 3 THEN
            EXIT FOR
        END IF
        PRINT "inner ", i
        EXIT DO
    LOOP
NEXT i

' GOTO
GOTO skip
PRINT "should not print"
skip:
PRINT "after goto"
