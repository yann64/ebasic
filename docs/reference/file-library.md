# File Library

eBasic's standard file creation/modification/deletion procedures -
`KILL`/`MKDIR`/`RMDIR`/etc., matching FreeBASIC's own *filesystem-level*
set. Like the [String Library](string-library.md), these aren't reserved
keywords - they're ordinary functions, always pre-declared (via the same
hidden `Extern "C++"` block the compiler splices into every program
before your own source), callable from any `.bas` file with no
`#include` needed:

```basic
DIM ok AS INTEGER
ok = WriteFile("greeting.txt", "hello")
PRINT FileExists("greeting.txt")    ' -1 (TRUE)
PRINT FileLen("greeting.txt")       ' 5
```

Because they're pre-declared this way, a program can't declare its own
`SUB`/`FUNCTION`/variable under any of these exact names (a real
"already declared" error) - see the String Library's own note on this.

**Scope**: plain filesystem operations only - create/delete/rename/copy a
file, create/remove a directory, check existence/size, read/write a
whole file's contents. This is **not** FreeBASIC's much larger `OPEN`/
`PRINT #`/`INPUT #`/`GET`/`PUT`/`SEEK` streaming-file-handle API (real
BASIC statements with handle management and random/binary access) -
genuinely bigger scope, not covered here.

**Every function returns a plain `INTEGER` status** (nonzero/`TRUE` on
success, `0`/`FALSE` on failure) rather than raising an error - eBasic has
no `ON ERROR`/`TRY` construct to raise one into, so a missing file, a
permissions problem, or any other real filesystem error is always a
returned failure status, never a crash. Relative paths resolve against
the running program's current working directory, same as any other
language.

## `FileExists`

```
FileExists(path AS STRING) AS INTEGER
```

Whether `path` exists (a file, directory, or anything else the
filesystem has at that path).

## `FileLen`

```
FileLen(path AS STRING) AS LONGINT
```

The file's size in bytes, or `-1` if it doesn't exist (or otherwise
can't be examined) - distinguished from a genuinely empty file, which is
a valid `0`.

## `Kill`

```
Kill(path AS STRING) AS INTEGER
```

Deletes a file. Fails (returns `0`) if `path` doesn't exist.

## `MkDir`

```
MkDir(path AS STRING) AS INTEGER
```

Creates one new directory level. Fails if `path` already exists (matching
real FreeBASIC's own `MkDir`) or if its parent doesn't exist yet -
`MkDir` never creates missing parent directories ("mkdir -p" is out of
scope).

## `RmDir`

```
RmDir(path AS STRING) AS INTEGER
```

Removes an *empty* directory. Fails on a non-empty one, or on a plain
file (checked explicitly - this never silently deletes a file the way a
more generic "remove" might).

## `Rename`

```
Rename(oldPath AS STRING, newPath AS STRING) AS INTEGER
```

Renames (or moves, if the two paths are in different directories) a file
or directory. This is FreeBASIC's own `Name oldfile As newfile`
statement, as an ordinary function under a different name - `Name` isn't
usable here, since it's already an extremely common identifier
(`DIM name AS STRING` and similar are everywhere) and reserving it
globally would break that.

```basic
PRINT Rename("draft.bas", "final.bas")
```

## `FileCopy`

```
FileCopy(source AS STRING, destination AS STRING) AS INTEGER
```

Copies `source` to `destination`, overwriting `destination` if it
already exists.

## `ReadFile`

```
ReadFile(path AS STRING, BYREF ok AS INTEGER) AS STRING
```

Reads `path`'s entire contents. `ok` (an out-parameter) reports whether
the read actually succeeded - a genuinely empty file and a failed read
both return `""`, so check `ok`, not just the result, to tell them apart.

```basic
DIM ok AS INTEGER
DIM contents AS STRING
contents = ReadFile("notes.txt", ok)
IF ok <> 0 THEN
    PRINT contents
END IF
```

## `WriteFile`

```
WriteFile(path AS STRING, contents AS STRING, append AS INTEGER = 0) AS INTEGER
```

Writes `contents` to `path`, overwriting any existing content by
default; pass a nonzero `append` to add to the end of the file instead
(see [Default parameter values](procedures-and-arrays.md#default-parameter-values)
- most callers never need to pass this at all).

```basic
CALL WriteFile("log.txt", "started" & Chr(10))
CALL WriteFile("log.txt", "finished" & Chr(10), 1)   ' appended
```

## See also

- [String Library](string-library.md)
- [Procedures and Arrays: Default parameter values](procedures-and-arrays.md#default-parameter-values)
