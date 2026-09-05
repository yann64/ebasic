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
function pointer, typed as `ANY PTR` - matching how C callback-style APIs
conventionally take a callback as an untyped pointer (e.g. GLib's own
`GCallback`, used by every `g_signal_connect`-family GTK function). This
lets separately-compiled C/C++ code call back into eBasic-compiled code,
the mirror image of an ordinary `Extern` `Declare` (which lets eBasic call
into external code).

- Only a **plain, bodied, top-level** `SUB`/`FUNCTION` is addressable this
  way - not an `Extern`-declared one (it has no eBasic-compiled body to
  take the address of) and not a `TYPE` method (which needs an implicit
  `This` a plain C function pointer has no room for).
- Every parameter, and the return type (for a `FUNCTION`), must be
  C-ABI-compatible - `STRING` isn't (it's a C++ class, not a C-layout
  value); use `ZSTRING` instead, same as an ordinary `Extern` signature.
- Deliberately narrow scope: produces `ANY PTR`, not a distinct,
  structurally-checked function-pointer *type* - the receiving C parameter
  can be typed however that library's own real header declares it (a
  function-pointer typedef, `void*`, ...); eBasic never inspects real
  headers regardless (see above), so only the *value* needs to line up,
  not a matching declared type on the eBasic side.

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
