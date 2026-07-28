''' A greeting message length.
CONST GreetingLength AS INTEGER = 5

''' Doubles a number.
FUNCTION Twice(n AS INTEGER) AS INTEGER
    Twice = n * 2
END FUNCTION

' plain comment, not a doc comment - should not be attached
FUNCTION Triple(n AS INTEGER) AS INTEGER
    Triple = n * 3
END FUNCTION
