#pragma once

#include <string>

namespace ebasic {

/// The name registered for the builtin prelude's own synthetic "file" (via
/// DiagnosticEngine::registerFile) - Sema uses this to recognize a
/// statement as coming from the prelude rather than real user source (see
/// its own `collectExternSignatureChecks`), so its `STRING`-in-signature
/// exemption applies only here, never to a user-written `Extern`.
inline const char* kBuiltinPreludeFileName = "<builtin>";

/// A hardcoded eBasic source string, spliced in (via the same
/// `expandSource` mechanism `#include` itself uses - see preprocess())
/// before every compiled program's own source - eBasic's standard
/// library, always available, no `#include` needed. Declares each
/// function's signature only (`Extern "C++"`, no `Lib` clause - nothing
/// to link, no `Namespace` block - callable bare); the real
/// implementations are ordinary C++ functions in
/// runtime/include/ebasic/runtime/{stringlib,filelib}.hpp, already
/// `#include`d by every generated program's own runtime.hpp - so no
/// linking step is needed for any of this either.
///
/// Every `STRING` parameter is explicitly `BYVAL` - `STRING` defaults to
/// `BYREF` otherwise, and a `BYREF` parameter can't have a default value
/// (see docs/reference/procedures-and-arrays.md); `INTEGER`/`DOUBLE`
/// already default to `BYVAL`, so they're left implicit here (except
/// `ReadFile`'s own `ok` out-parameter, which needs `BYREF` explicitly).
inline const std::string kBuiltinPreludeSource = R"EBASIC_PRELUDE(
Extern "C++"
    ' The string library (LEN/MID/LEFT/RIGHT/INSTR/etc., matching
    ' FreeBASIC's own set) - see docs/reference/string-library.md.
    Declare Function Len(BYVAL s AS STRING) AS INTEGER
    Declare Function Left(BYVAL s AS STRING, count AS INTEGER) AS STRING
    Declare Function Right(BYVAL s AS STRING, count AS INTEGER) AS STRING
    Declare Function Mid(BYVAL s AS STRING, start AS INTEGER, length AS INTEGER = 2147483647) AS STRING
    Declare Function InStr(BYVAL haystack AS STRING, BYVAL needle AS STRING, start AS INTEGER = 1) AS INTEGER
    Declare Function InStrRev(BYVAL haystack AS STRING, BYVAL needle AS STRING, start AS INTEGER = -1) AS INTEGER
    Declare Function UCase(BYVAL s AS STRING) AS STRING
    Declare Function LCase(BYVAL s AS STRING) AS STRING
    Declare Function LTrim(BYVAL s AS STRING, BYVAL chars AS STRING = " ") AS STRING
    Declare Function RTrim(BYVAL s AS STRING, BYVAL chars AS STRING = " ") AS STRING
    Declare Function Trim(BYVAL s AS STRING, BYVAL chars AS STRING = " ") AS STRING
    Declare Function Str(n AS DOUBLE) AS STRING
    Declare Function Val(BYVAL s AS STRING) AS DOUBLE
    Declare Function Chr(code AS INTEGER) AS STRING
    Declare Function Asc(BYVAL s AS STRING) AS INTEGER
    Declare Function Space(n AS INTEGER) AS STRING
    Declare Function Repeat(n AS INTEGER, BYVAL s AS STRING) AS STRING

    ' The file library (KILL/MKDIR/RMDIR/NAME-as-Rename/etc., matching
    ' FreeBASIC's own filesystem-level set) - see
    ' docs/reference/file-library.md. Every function returns a plain
    ' INTEGER status (nonzero = success) - eBasic has no error-handling
    ' construct to raise a real error into.
    Declare Function FileExists(BYVAL path AS STRING) AS INTEGER
    Declare Function FileLen(BYVAL path AS STRING) AS LONGINT
    Declare Function Kill(BYVAL path AS STRING) AS INTEGER
    Declare Function MkDir(BYVAL path AS STRING) AS INTEGER
    Declare Function RmDir(BYVAL path AS STRING) AS INTEGER
    Declare Function Rename(BYVAL oldPath AS STRING, BYVAL newPath AS STRING) AS INTEGER
    Declare Function FileCopy(BYVAL source AS STRING, BYVAL destination AS STRING) AS INTEGER
    Declare Function ReadFile(BYVAL path AS STRING, BYREF ok AS INTEGER) AS STRING
    Declare Function WriteFile(BYVAL path AS STRING, BYVAL contents AS STRING, append AS INTEGER = 0) AS INTEGER

    ' The math library (ABS/SGN/SQR/trig/CInt-family/HEX$-family, matching
    ' FreeBASIC's own set) - see docs/reference/math-library.md.
    Declare Function Abs(n AS DOUBLE) AS DOUBLE
    Declare Function Sgn(n AS DOUBLE) AS INTEGER
    Declare Function Sqr(n AS DOUBLE) AS DOUBLE
    Declare Function Sin(n AS DOUBLE) AS DOUBLE
    Declare Function Cos(n AS DOUBLE) AS DOUBLE
    Declare Function Tan(n AS DOUBLE) AS DOUBLE
    Declare Function Asin(n AS DOUBLE) AS DOUBLE
    Declare Function Acos(n AS DOUBLE) AS DOUBLE
    Declare Function Atn(n AS DOUBLE) AS DOUBLE
    Declare Function Atan2(y AS DOUBLE, x AS DOUBLE) AS DOUBLE
    Declare Function Exp(n AS DOUBLE) AS DOUBLE
    Declare Function Log(n AS DOUBLE) AS DOUBLE
    Declare Function Int(n AS DOUBLE) AS DOUBLE
    Declare Function Fix(n AS DOUBLE) AS DOUBLE
    Declare Function Rnd(n AS DOUBLE = 1) AS DOUBLE
    Declare Sub Randomize(seed AS DOUBLE = 0)
    Declare Function CByte(n AS DOUBLE) AS BYTE
    Declare Function CUByte(n AS DOUBLE) AS UBYTE
    Declare Function CShort(n AS DOUBLE) AS SHORT
    Declare Function CUShort(n AS DOUBLE) AS USHORT
    Declare Function CInt(n AS DOUBLE) AS INTEGER
    Declare Function CUInt(n AS DOUBLE) AS UINTEGER
    Declare Function CLngInt(n AS DOUBLE) AS LONGINT
    Declare Function CULngInt(n AS DOUBLE) AS ULONGINT
    Declare Function CSng(n AS DOUBLE) AS SINGLE
    Declare Function CDbl(n AS DOUBLE) AS DOUBLE
    Declare Function CBool(n AS DOUBLE) AS BOOLEAN
    Declare Function Hex(n AS LONGINT) AS STRING
    Declare Function Oct(n AS LONGINT) AS STRING
    Declare Function Bin(n AS LONGINT) AS STRING
End Extern
)EBASIC_PRELUDE";

} // namespace ebasic
