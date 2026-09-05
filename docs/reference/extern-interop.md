# `EXTERN` / C-C++ Interop

Binds eBasic code to a real, separately-compiled C or C++ library - no body
is written; the real implementation lives externally, and the generated C++
declares a matching prototype and links against it (via `-l<name>` from a
`Lib` clause).

## `Extern "C" ... End Extern`

```
Extern "C" Lib "libname"
    Declare Sub Name(params...)
    Declare Function Name(params...) AS ReturnType
    ...
End Extern
```

Each `Declare` line inside the block is a bodyless signature bound to a real
C symbol of the same name (unless overridden with `Alias`, below). `Lib
"libname"` names the library to link (`-llibname`, no `lib`/`.so`/`.a`
affixes) - every `Declare` in the block shares it.

```basic
Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_add(ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
End Extern

PRINT eb_fixture_add(2, 3)   ' 5
```

## Standalone `Declare` (no block)

```
Declare Sub Name [Cdecl|Stdcall] [Lib "libname"] [Alias "realName"] (params...)
Declare Function Name [Cdecl|Stdcall] [Lib "libname"] [Alias "realName"] (params...) AS ReturnType
```

A `Declare` doesn't have to sit inside an `Extern` block - written at the
top level on its own, it always binds with `"C"` linkage (a standalone
`Declare` can't produce `"C++"` linkage; that's only reachable via an
`Extern "C++"` block). **The clause order matters**: `Cdecl`/`Stdcall`/
`Lib`/`Alias` all come *before* the parameter list, matching real
FreeBASIC's own grammar - `Lib "name" (params)`, not `(params) Lib
"name"`.

```basic
Declare Function eb_fixture_add Lib "ebfixturec" (ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
PRINT eb_fixture_add(2, 3)   ' 5
```

## `Cdecl` and `Stdcall` (calling convention)

`Cdecl` (the default - explicit `Cdecl` is a no-op) and `Stdcall` both
imply `"C"` linkage, the same as writing the `Declare` inside an `Extern
"C"` block - real Win32 APIs (`User32`, `GDI32`, `Kernel32`, ...) are
always `extern "C" __stdcall`, never C++-mangled:

```basic
Extern "C" Lib "user32"
    Declare Function MessageBoxA Stdcall (ByVal hWnd AS ANY PTR, ByVal text AS ZSTRING, ByVal caption AS ZSTRING, ByVal utype AS INTEGER) AS INTEGER
End Extern
```

A calling convention is a real ABI detail (who cleans the stack, not just
naming) - `Stdcall` on the eBasic side must match how the real function
was actually compiled, the same way getting `Lib`/`Alias` wrong would
produce a linker (not a runtime) error, except a calling-convention
mismatch can compile *and link* fine while still corrupting the stack at
runtime, particularly on 32-bit x86. `Stdcall` is meaningless (and a
no-op, not an error) on any target other than 32/64-bit Windows -
`__stdcall` isn't even a distinct calling convention on Linux/macOS/Haiku
or on 64-bit Windows itself (x64's own calling convention is unified), so
writing it in a `.bas` file meant to also build elsewhere is always safe.

## `Alias`

Binds an eBasic-side name to a *different* real external symbol name -
needed whenever the real name isn't a valid/desired eBasic identifier, or to
give two differently-typed eBasic overloads of the same real C++ overloaded
function:

```basic
Extern "C++" Lib "ebfixturecpp"
    Namespace ebfixture
        Declare Function AddInts Alias "Add" (ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
        Declare Function AddDoubles Alias "Add" (ByVal a AS DOUBLE, ByVal b AS DOUBLE) AS DOUBLE
    End Namespace
End Extern

PRINT ebfixture.AddInts(2, 3)        ' 5
PRINT ebfixture.AddDoubles(2.5, 3.5) ' 6
```

## `Extern "C++"` and `Namespace` (nested)

```
Extern "C++" Lib "libname"
    Namespace RealNamespaceName
        Declare Function Name(params...) AS ReturnType
    End Namespace
End Extern
```

A `Namespace` block nested inside `Extern "C++"` binds to a real C++
namespaced function - called from eBasic the same way as an ordinary
[`NAMESPACE`](namespaces-pointers-unions.md#namespace) member,
`RealNamespaceName.Name(...)`.

```basic
Extern "C++" Lib "ebfixturecpp"
    Namespace ebfixture
        Declare Function Square(ByVal x AS INTEGER) AS INTEGER
    End Namespace
End Extern

PRINT ebfixture.Square(5)   ' 25
```

## `ZSTRING` at the interop boundary

A `STRING` argument converts to/from a `ZSTRING`/`ZSTRING PTR` parameter
automatically - no manual marshaling needed at the call site:

```basic
Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_greeting() AS ZSTRING
    Declare Function eb_fixture_strlen_like(ByVal s AS ZSTRING) AS INTEGER
End Extern

DIM greeting AS STRING
greeting = eb_fixture_greeting()          ' ZSTRING (const char*) -> STRING
PRINT greeting

DIM mine AS STRING
mine = "hello there"
PRINT eb_fixture_strlen_like(mine)        ' STRING -> ZSTRING
```

A C function documented to return `NULL` on failure converts to an empty
`STRING`, not a crash - `ZSTRING`-returning calls are null-safe by
construction.

## Opaque "handle" types

```
TYPE Handle
END TYPE
```

A `TYPE` with **zero fields and zero methods** is an opaque external handle
- the classic C "pointer to an incomplete struct" idiom (as used by SQLite,
SDL, and many other real C libraries), where the caller only ever holds a
pointer and never needs (or is allowed) to know the real layout. It may
**only** be used behind a `PTR` - a by-value `DIM` of an opaque type is a
Sema error (`cannot be a by-value DIM of opaque external TYPE '<name>'
(unknown layout - declare it as '<name> PTR' instead)`).

```basic
TYPE Handle
END TYPE

Extern "C" Lib "ebfixturec"
    Declare Function HandleCreate Alias "eb_fixture_handle_create" (ByVal initial AS INTEGER) AS Handle PTR
    Declare Function HandleGet Alias "eb_fixture_handle_get" (ByVal h AS Handle PTR) AS INTEGER
    Declare Sub HandleDestroy Alias "eb_fixture_handle_destroy" (ByVal h AS Handle PTR)
End Extern

DIM h AS Handle PTR
h = HandleCreate(10)
PRINT HandleGet(h)     ' 10
CALL HandleDestroy(h)
```

## `@ProcName` - a function pointer for a C callback API

```basic
SUB OnFixtureCallback(value AS INTEGER, user_data AS ANY PTR)
    PRINT "callback fired"
    PRINT value
End Sub

Extern "C" Lib "ebfixturec"
    Declare Sub eb_fixture_invoke_callback(ByVal cb AS ANY PTR, ByVal value AS INTEGER, ByVal user_data AS ANY PTR)
End Extern

CALL eb_fixture_invoke_callback(@OnFixtureCallback, 42, 0)
```

`@` (AddressOf) accepts a top-level, non-`Extern`, non-method `SUB`/
`FUNCTION` name (not just an lvalue, its ordinary use - see [Pointers](
namespaces-pointers-unions.md#pointers-ptr--)) and produces a real C
function pointer. This lets separately-compiled C/C++ code call back into
eBasic-compiled code, the mirror image of an ordinary `Extern` `Declare`
(which lets eBasic call into external code).

- Only a **plain, bodied, top-level** `SUB`/`FUNCTION` is addressable this
  way - not an `Extern`-declared one (it has no eBasic-compiled body to
  take the address of) and not a `TYPE` method (which needs an implicit
  `This` a plain C function pointer has no room for).
- Every parameter, and the return type (for a `FUNCTION`), must be
  C-ABI-compatible - `STRING` isn't (it's a C++ class, not a C-layout
  value); use `ZSTRING` instead, same as an ordinary `Extern` signature.
- The parameter it's assigned/passed to may be typed either as `ANY PTR`
  (the example above - the receiving C parameter can be typed however
  that library's own real header declares it, and eBasic never inspects
  real headers, so only the *value* needs to line up) **or** as a real,
  structurally-checked function-pointer type (below) - `@ProcName`
  produces whichever shape the destination needs.

### Typed function-pointer parameters, DIMs, and TYPE fields

```
SUB (ByVal AS Type, ...)
FUNCTION [Cdecl|Stdcall] (ByVal AS Type, ...) AS ReturnType
```

A parameter, `DIM`, or `TYPE` field may be typed as a real function
pointer - `SUB (...)` for a callback with no return value, `FUNCTION (...)
AS ReturnType` for one that returns a value. Each entry inside the
parentheses is written the same way FreeBASIC itself writes an anonymous
parameter - `ByVal AS Type` (the name is omitted; `ByVal` may be too, it's
always implied). Unlike `ANY PTR`, Sema checks the assigned/passed value's
*actual signature* against the declared type - calling convention,
parameter types, and return type must all match:

```basic
Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_invoke_comparator Cdecl (ByVal cmp AS FUNCTION (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER, ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
End Extern

FUNCTION Compare(BYVAL a AS INTEGER, BYVAL b AS INTEGER) AS INTEGER
    IF a < b THEN
        RETURN -1
    ELSEIF a > b THEN
        RETURN 1
    ELSE
        RETURN 0
    END IF
END FUNCTION

PRINT eb_fixture_invoke_comparator(@Compare, 3, 7)   ' -1

' Also storable, not just usable inline:
DIM cb AS FUNCTION (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER
cb = @Compare
PRINT eb_fixture_invoke_comparator(cb, 7, 3)         ' 1
```

A typed function pointer and a bare `ANY PTR` are freely bridgeable in
both directions (assigning one to the other needs no cast on the eBasic
side) - this is what keeps every existing untyped `@ProcName` use working
unchanged, and lets a typed callback flow through code that only knows
about `ANY PTR` (e.g. storing it in a generic `void*`-shaped field for
later use).

### Calling through a stored function pointer

A typed function pointer isn't only for handing off to external code -
`cb(...)` calls straight through it, whether `cb` is a `DIM`'d variable,
a parameter (the most useful case - a real higher-order function), or a
`TYPE` field via a qualified receiver:

```basic
FUNCTION ApplyTwice(BYVAL f AS FUNCTION (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER, BYVAL x AS INTEGER) AS INTEGER
    RETURN f(f(x, 1), x)
END FUNCTION

PRINT ApplyTwice(@Compare, 5)   ' -1

DIM cb AS FUNCTION (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER
cb = @Compare
PRINT cb(3, 7)                  ' expression position: -1
CALL cb(3, 7)                   ' statement position, return value discarded

TYPE Callbacks
    cmp AS FUNCTION (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER
END TYPE

DIM c AS Callbacks
c.cmp = @Compare
PRINT c.cmp(3, 7)                ' a TYPE field, via a qualified receiver: -1
```

A `SUB (...)`-shaped (no return value) pointer may be called as a
statement (`CALL cb(...)`) but not used in an expression - the same rule
an ordinary `SUB` call already follows. Not supported: calling through a
`PROPERTY` of function-pointer type (`f.SomeProp(1, 2)`, where `SomeProp`
is declared via `Declare Property`, not a plain field) - only a plain
`TYPE` field.

### `Stdcall` on an eBasic-defined callback

A plain top-level `SUB`/`FUNCTION` may itself be marked `Stdcall`, right
after its name (same position as `Extern`/`Declare`'s own
`Cdecl`/`Stdcall` clause):

```basic
FUNCTION Compare Stdcall (BYVAL a AS INTEGER, BYVAL b AS INTEGER) AS INTEGER
    ...
END FUNCTION
```

`@Compare`'s resulting function-pointer type now carries `Stdcall` too,
so it can be handed to a real Win32 API expecting a `Stdcall` callback
(`EnumWindows`, `SetTimer`) - relevant, as a genuine ABI difference, only
on 32-bit x86 Windows; a harmless no-op everywhere else, same as
`Extern`/`Declare`'s own `Stdcall`. Not supported on a `TYPE` method's
out-of-line definition (`SUB TypeName.MethodName Stdcall (...)` is
rejected) - a method already carries an implicit `This`/vtable slot a
plain C function pointer has no room for.

## Exporting eBasic code (`Extern "C"` with a real body)

Everything above is eBasic *calling into* external code. The reverse
direction - external code calling *into* eBasic - uses the same `Extern
"C" ... End Extern` syntax, just with a real, bodied `SUB`/`FUNCTION`
definition instead of a bodyless `Declare`:

```basic
Extern "C"
    Function AddNumbers(a AS INTEGER, b AS INTEGER) AS INTEGER
        AddNumbers = a + b
    End Function
End Extern
```

Only meaningful for [`ebc --shared-lib`](../guide/ebc.md#--shared-lib-mode)
(building a real, dynamically loadable shared library) - it's what gives a
function a stable, unmangled C symbol another program can `dlopen`/
`dlsym` (or `LoadLibrary`/`GetProcAddress`) by name, instead of `ebc`'s
usual internal mangled name. The same C-ABI restrictions apply as an
ordinary `Extern`/`Declare` signature (no `STRING`; use `ZSTRING`
instead), and only `Extern "C"` (not `Extern "C++"`) may contain a bodied
definition - a mangled C++-linkage "export" isn't a stable ABI boundary.
An ordinary top-level `SUB`/`FUNCTION` elsewhere in the same file is
unaffected and keeps using `STRING`/`TYPE`s freely - only the opted-in
export crosses the C-ABI boundary.

## See also

- [Types and Literals](types-and-literals.md) - `ZSTRING`
- [Namespaces, Pointers, and Unions](namespaces-pointers-unions.md) - `PTR`, `NAMESPACE`
- [End-user guide: `ebpm`](../guide/ebpm.md) - linking against a package's own auto-generated interface uses this exact same `Extern "C++"`/`Declare` machinery
- [End-user guide: `ebc`](../guide/ebc.md) - `--shared-lib` mode, the only place a bodied `Extern "C"` export is meaningful
