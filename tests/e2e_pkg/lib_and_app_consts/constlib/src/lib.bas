' A top-level CONST/ENUM must be exported across a --lib boundary too - a
' downstream consumer needs these (e.g. a GTK4 binding's orientation/flag
' constants) just as much as it needs functions/TYPEs.

CONST MaxRetries = 3

' A negative-literal CONST (e.g. a real GTK GtkResponseType value like
' GTK_RESPONSE_ACCEPT = -3) must be exported too - it parses as
' UnaryNeg(IntLiteral), not a single literal node, so this is a real,
' distinct case from a bare positive literal.
CONST MinBalance = -100

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
