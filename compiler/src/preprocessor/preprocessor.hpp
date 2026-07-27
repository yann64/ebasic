#pragma once

#include "diagnostics/diagnostics.hpp"

#include <string>

namespace ebasic {

// A line-based textual preprocessing pass run on the raw source before
// lexing: object-like #define (no parameters) and #ifdef/#ifndef/#else/
// #endif conditional compilation. Macro names are case-sensitive (unlike
// the rest of the case-insensitive BASIC language). #include is not
// supported yet - it's scheduled as part of M2's multi-file program support.
//
// Excluded/replaced text is blanked rather than removed so line numbers -
// and therefore diagnostics from later stages - stay accurate.
std::string preprocess(const std::string& source, DiagnosticEngine& diags);

} // namespace ebasic
