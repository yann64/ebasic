# Process Library

eBasic's standard process/environment/program-control procedures -
`Environ`/`Command`/`Shell`/`Sleep`/`ExitProcess`, matching FreeBASIC's
own set. Like the [Math Library](math-library.md), these aren't
reserved keywords - they're ordinary functions, always pre-declared,
callable from any `.bas` file with no `#include` needed.

Because they're pre-declared this way, a program can't declare its own
`SUB`/`FUNCTION`/variable under any of these exact names (a real
"already declared" error) - see the String Library's own note on this.

## `Environ`

```
Environ(name AS STRING) AS STRING
```

Reads an environment variable - `""` if it's unset.

## `Command`

```
Command() AS STRING
```

The program's own command-line arguments, space-joined into a single
string - not including the program's own path, matching real
FreeBASIC's own `Command$`. `""` if the program was run with no
arguments.

```basic
PRINT Command()    ' e.g. "--verbose input.txt", or "" with no arguments
```

## `Shell`

```
Shell(command AS STRING) AS INTEGER
```

Runs `command` via the system shell, returning its exit status (`0`
typically means success, matching any other shell's own convention) -
or `-1` if the shell itself couldn't be started. The *safety* of what a
program passes here is the same consideration as any other language's
shell-out capability (`system()`/`exec()`) - this library doesn't add
its own sandboxing.

```basic
PRINT Shell("ls -la")
```

## `Sleep`

```
Sleep(milliseconds AS INTEGER)
```

Blocks the calling program for `milliseconds`. Unlike real FreeBASIC's
own `Sleep`, there's no no-argument/negative "wait for a keypress" form
- console input is out of scope for this library (see the Math
Library's own note on why console/terminal control isn't implemented at
all). A non-positive value returns immediately.

## `ExitProcess`

```
ExitProcess(code AS INTEGER = 0)
```

Terminates the program immediately with the given exit code - no
statement after it in the same `SUB`/`FUNCTION`/top-level code ever
runs. This is deliberately a different name from the `END` keyword:
`END` always closes a block (`END IF`/`END SUB`/`END FUNCTION`/...) and
never means "stop the program" on its own.

```basic
PRINT "starting"
IF Shell("some-check") <> 0 THEN
    PRINT "check failed"
    CALL ExitProcess(1)
END IF
PRINT "check passed"
```

## See also

- [Math Library](math-library.md)
- [Date/Time Library](date-time-library.md)
