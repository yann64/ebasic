#define GRID_SIZE 4
#define GREETING "hi"

' CONST
CONST PI AS SINGLE = 3.5
CONST MAX = 100 * 2
PRINT PI
PRINT MAX
PRINT GREETING

' ENUM with auto-increment, an explicit override, and resumed auto-increment
ENUM Color
    Red
    Green
    Blue
    Purple = 10
    Pink
END ENUM

PRINT Red
PRINT Green
PRINT Blue
PRINT Purple
PRINT Pink

' static arrays: default 0-based bound, explicit TO bound, GRID_SIZE from #define
DIM squares(GRID_SIZE) AS INTEGER
DIM i AS INTEGER
FOR i = 0 TO GRID_SIZE
    squares(i) = i * i
NEXT i
FOR i = 0 TO GRID_SIZE
    PRINT squares(i)
NEXT i

DIM names(1 TO 3) AS STRING
names(1) = "one"
names(2) = "two"
names(3) = "three"
PRINT names(1)
PRINT names(2)
PRINT names(3)

#ifdef GRID_SIZE
PRINT "grid size is defined"
#endif

#ifndef NOT_DEFINED
PRINT "not_defined is not defined"
#else
PRINT "should not print"
#endif
