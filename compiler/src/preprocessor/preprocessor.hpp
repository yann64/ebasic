#pragma once

#include "diagnostics/diagnostics.hpp"

#include <string>
#include <vector>

namespace ebasic {

/// The result of preprocessing: `source` is the fully flattened, macro- and
/// #include-expanded text ready for the Lexer, and `lineMap[i]` gives the
/// true originating {fileId, line} of `source`'s (i+1)-th line - since a
/// multi-file program's flattened text no longer corresponds 1:1 to any
/// single file's line numbers, downstream diagnostics look up the real
/// location here rather than using their own flattened-line counter directly.
struct PreprocessResult {
    std::string source;
    std::vector<SourceLoc> lineMap;
};

/// A line-based textual preprocessing pass run on the raw source before
/// lexing: object-like #define (no parameters), #ifdef/#ifndef/#else/#endif
/// conditional compilation, and #include/#include once (recursively, with
/// circular-include detection). Macro names are case-sensitive (unlike the
/// rest of the case-insensitive BASIC language), and so are directive
/// keywords themselves (#define, #include, once, ...).
///
/// Excluded/replaced text is blanked rather than removed, and #include'd
/// content is spliced in place, so that `lineMap` - and therefore
/// diagnostics from later stages - always points at a real source location.
///
/// `mainSource` is the already-read content of `mainPath` (the file main.cpp
/// opened); nested #include targets are read internally as encountered.
///
/// `includeDirs` (M5): extra search paths consulted, in order, as a fallback
/// only when a quoted #include target can't be found relative to the
/// including file's own directory - never overriding that includer-relative
/// lookup, exactly like a C/C++ compiler's own `-I` list only ever backs up
/// the quote-form's primary, includer-relative search. Lets a package depend
/// on another package's directory (e.g. an auto-generated `.iface.bas`)
/// without needing to know its exact relative filesystem path.
PreprocessResult preprocess(const std::string& mainSource, const std::string& mainPath,
                            DiagnosticEngine& diags,
                            const std::vector<std::string>& includeDirs = {});

} // namespace ebasic
