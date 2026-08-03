' eBasic's standard date/time library (see
' docs/reference/date-time-library.md) - DateSerial/TimeSerial round-
' trips (including a leap day and a pre-epoch/pre-1970 date), Weekday
' against two known real weekdays, DateAdd's calendar-clamping behavior
' (month-end and leap-day clamps, plus a negative-months carry across a
' year boundary), and DateDiff's calendar-boundary-crossing semantics
' for yyyy/m vs. exact-duration semantics for d/h/n/s. Now()/Date()/
' Time()/Timer() are non-deterministic (today's wall clock), so instead
' of a golden value they're checked at the end via deterministic
' invariants (a plausible year, a correctly-formatted string length, an
' in-range Timer()).

DIM y2000 AS DOUBLE
y2000 = DateSerial(2000, 1, 1)
PRINT Year(y2000)
PRINT Month(y2000)
PRINT Day(y2000)
PRINT Weekday(y2000)

DIM leap AS DOUBLE
leap = DateSerial(2024, 2, 29)
PRINT Year(leap)
PRINT Month(leap)
PRINT Day(leap)
PRINT Weekday(leap)

PRINT DateSerial(1899, 12, 30)
PRINT DateSerial(1970, 1, 1)

DIM t AS DOUBLE
t = TimeSerial(13, 30, 45)
PRINT Hour(t)
PRINT Minute(t)
PRINT Second(t)

DIM combo AS DOUBLE
combo = DateSerial(2020, 6, 15) + TimeSerial(23, 59, 59)
PRINT Year(combo)
PRINT Month(combo)
PRINT Day(combo)
PRINT Hour(combo)
PRINT Minute(combo)
PRINT Second(combo)

DIM jan31 AS DOUBLE
jan31 = DateSerial(2023, 1, 31)
DIM afterMonth AS DOUBLE
afterMonth = DateAdd("m", 1, jan31)
PRINT Year(afterMonth)
PRINT Month(afterMonth)
PRINT Day(afterMonth)

DIM afterYear AS DOUBLE
afterYear = DateAdd("yyyy", 1, leap)
PRINT Year(afterYear)
PRINT Month(afterYear)
PRINT Day(afterYear)

DIM dec1 AS DOUBLE
dec1 = DateSerial(2023, 12, 1)
DIM minusMonths AS DOUBLE
minusMonths = DateAdd("m", -13, dec1)
PRINT Year(minusMonths)
PRINT Month(minusMonths)
PRINT Day(minusMonths)

DIM dec31_2020 AS DOUBLE
dec31_2020 = DateSerial(2020, 12, 31)
DIM jan1_2021 AS DOUBLE
jan1_2021 = DateSerial(2021, 1, 1)
PRINT DateDiff("yyyy", dec31_2020, jan1_2021)
PRINT DateDiff("d", dec31_2020, jan1_2021)
PRINT DateDiff("m", dec31_2020, jan1_2021)

DIM day1 AS DOUBLE
day1 = DateSerial(2020, 1, 1)
DIM day2 AS DOUBLE
day2 = DateSerial(2020, 1, 2)
PRINT DateDiff("h", day1, day2)
PRINT DateDiff("n", day1, day2)
PRINT DateDiff("s", day1, day2)

DIM before AS DOUBLE
before = DateSerial(1850, 3, 10)
PRINT Year(before)
PRINT Month(before)
PRINT Day(before)

PRINT Year(Now()) >= 2020
PRINT Len(Date()) = 10
PRINT Len(Time()) = 8
PRINT Timer() >= 0
PRINT Timer() < 86400

PRINT "date/time library ok"
