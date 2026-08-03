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
/// before every compiled program's own source - eBasic's standard string
/// library (LEN/MID/LEFT/RIGHT/INSTR/etc., matching FreeBASIC's own set;
/// see docs/reference/string-library.md), always available, no `#include`
/// needed. Declares each function's signature only (`Extern "C++"`, no
/// `Lib` clause - nothing to link, no `Namespace` block - callable bare);
/// the real implementations are ordinary C++ functions in
/// runtime/include/ebasic/runtime/stringlib.hpp, already `#include`d by
/// every generated program's own runtime.hpp - so no linking step is
/// needed for any of this either.
///
/// Every `STRING` parameter is explicitly `BYVAL` - `STRING` defaults to
/// `BYREF` otherwise, and a `BYREF` parameter can't have a default value
/// (see docs/reference/procedures-and-arrays.md); `INTEGER`/`DOUBLE`
/// already default to `BYVAL`, so they're left implicit here.
inline const std::string kBuiltinPreludeSource = R"EBASIC_PRELUDE(
Extern "C++"
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
End Extern
)EBASIC_PRELUDE";

} // namespace ebasic
