# Date/Time Library

eBasic's standard date/time procedures - `DateSerial`/`Year`/`Month`/
`Day`/`DateAdd`/`DateDiff`/etc., matching FreeBASIC's own set. Like the
[Math Library](math-library.md), these aren't reserved keywords -
they're ordinary functions, always pre-declared, callable from any
`.bas` file with no `#include` needed.

Because they're pre-declared this way, a program can't declare its own
`SUB`/`FUNCTION`/variable under any of these exact names (a real
"already declared" error) - see the String Library's own note on this.

## The serial number

A date+time is a plain `DOUBLE` "serial number" - whole days since
`1899-12-30` (matching real FreeBASIC's own internal convention
exactly), with the fractional part representing time-of-day. This is
what makes arithmetic on a serial trivial for day-and-smaller units:

```basic
DIM today AS DOUBLE
today = DateSerial(2024, 3, 1)
PRINT Year(today + 1)     ' the next day - 2024, still March
PRINT Day(today + 1)      ' 2
```

A date and a time combine by simple addition:

```basic
DIM meeting AS DOUBLE
meeting = DateSerial(2024, 3, 1) + TimeSerial(14, 30, 0)
PRINT Hour(meeting)    ' 14
```

## Building and reading a serial

```
DateSerial(year AS INTEGER, month AS INTEGER, day AS INTEGER) AS DOUBLE
TimeSerial(hour AS INTEGER, minute AS INTEGER, second AS INTEGER) AS DOUBLE
Year(serial AS DOUBLE) AS INTEGER
Month(serial AS DOUBLE) AS INTEGER
Day(serial AS DOUBLE) AS INTEGER
Hour(serial AS DOUBLE) AS INTEGER
Minute(serial AS DOUBLE) AS INTEGER
Second(serial AS DOUBLE) AS INTEGER
Weekday(serial AS DOUBLE) AS INTEGER
```

`Weekday` returns `1` (Sunday) through `7` (Saturday), matching real
FreeBASIC. All of these accept a date from any year - including years
before the 1899-12-30 epoch (a negative serial), which round-trips
correctly through `Year`/`Month`/`Day` like any other date.

## `Now` / `Timer` / `Date` / `Time`

```
Now() AS DOUBLE
Timer() AS DOUBLE
Date() AS STRING
Time() AS STRING
```

`Now` is the current local date+time as a serial. `Timer` is seconds
since midnight, local time. `Date`/`Time` format the current local
date/time as `"YYYY-MM-DD"`/`"HH:MM:SS"` - a deliberate ISO-8601-style
choice, **not** a reproduction of real FreeBASIC's own locale-dependent
legacy `mm-dd-yyyy` `Date$` format.

## `DateAdd` / `DateDiff`

```
DateAdd(intervalCode AS STRING, number AS DOUBLE, serial AS DOUBLE) AS DOUBLE
DateDiff(intervalCode AS STRING, startSerial AS DOUBLE, endSerial AS DOUBLE) AS DOUBLE
```

`intervalCode` matches real FreeBASIC's own string codes: `"yyyy"`
(year), `"m"` (month), `"d"` (day), `"h"` (hour), `"n"` (minute), `"s"`
(second).

`DateAdd`'s calendar units (`"yyyy"`/`"m"`) clamp the day-of-month if
the target month is shorter - adding a month to January 31st lands on
February 28th (or 29th in a leap year), not March 3rd - and always
preserve the original time-of-day exactly. The fixed-length units
(`"d"`/`"h"`/`"n"`/`"s"`) are plain serial arithmetic.

```basic
PRINT Day(DateAdd("m", 1, DateSerial(2023, 1, 31)))    ' 28
```

`DateDiff`'s calendar units count boundaries *crossed*, not elapsed
duration - matching real FreeBASIC/Visual Basic's own well-known
`DateDiff` behavior:

```basic
DIM a AS DOUBLE
a = DateSerial(2020, 12, 31)
DIM b AS DOUBLE
b = DateSerial(2021, 1, 1)
PRINT DateDiff("yyyy", a, b)    ' 1 - a year boundary was crossed...
PRINT DateDiff("d", a, b)       ' ...even though only 1 day elapsed
```

The fixed-length units (`"d"`/`"h"`/`"n"`/`"s"`) return the exact
elapsed duration in that unit instead.

## See also

- [Math Library](math-library.md)
- [File Library](file-library.md)
