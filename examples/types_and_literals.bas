' See docs/reference/types-and-literals.md

DIM age AS INTEGER
age = 30
PRINT age

DIM price AS SINGLE
price = 19.99
PRINT price

DIM greeting AS STRING
greeting = "eBasic"
PRINT greeting

DIM active AS BOOLEAN
active = TRUE
PRINT active

CONST MAX_USERS = 100
PRINT MAX_USERS

ENUM Direction
    North
    South
    East
    West
END ENUM
PRINT North
PRINT West

DIM scores(4) AS INTEGER
scores(0) = 10
scores(4) = 50
PRINT scores(0)
PRINT scores(4)
