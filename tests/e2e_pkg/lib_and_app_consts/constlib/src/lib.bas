' A top-level CONST/ENUM must be exported across a --lib boundary too - a
' downstream consumer needs these (e.g. a GTK4 binding's orientation/flag
' constants) just as much as it needs functions/TYPEs.

CONST MaxRetries = 3

ENUM Direction
    North
    East
    South
    West
END ENUM

FUNCTION IsRetryable(attempt AS INTEGER) AS INTEGER
    IF attempt < MaxRetries THEN
        IsRetryable = 1
    ELSE
        IsRetryable = 0
    END IF
END FUNCTION
