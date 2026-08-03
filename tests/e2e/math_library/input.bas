' eBasic's standard math library (see docs/reference/math-library.md) -
' trig/exp/log, Int vs Fix on both a positive and a negative value, a
' deterministic Randomize/Rnd reseed, every Cxxx numeric conversion
' (including an out-of-range case for the narrow integer types), and
' Hex/Oct/Bin including their own zero and negative edge cases.

PRINT Abs(-3.5)
PRINT Sgn(-5)
PRINT Sgn(0)
PRINT Sgn(5)
PRINT Sqr(9)
PRINT Sin(0)
PRINT Cos(0)
PRINT Tan(0)
PRINT Asin(1)
PRINT Acos(1)
PRINT Atn(1)
PRINT Atan2(1, 1)
PRINT Exp(1)
PRINT Log(2.718281828)
PRINT Int(1.5)
PRINT Int(-1.5)
PRINT Fix(1.5)
PRINT Fix(-1.5)

CALL Randomize(42)
DIM r1 AS DOUBLE
r1 = Rnd(1)
CALL Randomize(42)
DIM r2 AS DOUBLE
r2 = Rnd(1)
PRINT r1 = r2

PRINT CByte(200.9)
PRINT CByte(-200.9)
PRINT CUByte(200.9)
PRINT CShort(-1.9)
PRINT CUShort(70000.9)
PRINT CInt(-1.9)
PRINT CUInt(4294967295.0)
PRINT CLngInt(-1.9)
PRINT CULngInt(1.9)
PRINT CSng(3.14159265)
PRINT CDbl(3.14)
PRINT CBool(0)
PRINT CBool(5)
PRINT CBool(-3)

PRINT Hex(255)
PRINT Hex(0)
PRINT Hex(-1)
PRINT Oct(8)
PRINT Oct(0)
PRINT Bin(5)
PRINT Bin(0)

PRINT "math library ok"
