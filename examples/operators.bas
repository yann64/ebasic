' See docs/reference/operators.md

PRINT 2 + 3 * 4      ' 14 - * binds tighter than +
PRINT 7 / 2          ' 3.5 - always real division
PRINT 7 \ 2          ' 3 - integer division
PRINT 7 MOD 2        ' 1
PRINT 2 ^ 3          ' 8
PRINT -2 ^ 2         ' -4 - ^ binds tighter than unary -

PRINT 5 AND 3        ' 1
PRINT 5 OR 2         ' 7
PRINT 5 XOR 1        ' 4
PRINT NOT TRUE       ' 0

PRINT "abc" & "def"  ' abcdef
PRINT 3 > 2          ' -1
PRINT "abc" < "abd"  ' -1
