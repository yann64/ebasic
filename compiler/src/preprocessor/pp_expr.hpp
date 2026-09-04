#pragma once

#include <functional>
#include <optional>
#include <string>

namespace ebasic {

/// Evaluates a `#if`/`#elseif`/`#assert` expression, once `defined(...)` has
/// already been resolved to a literal 1/0 and every other macro name has
/// already been textually substituted (see preprocessor.cpp) - so this only
/// ever sees integer/string literals, parens, and the operators below,
/// never a bare macro name. A small recursive-descent evaluator,
/// deliberately not a general expression compiler: just enough for
/// conditional-compilation guards (`#if (DEBUG_LEVEL >= 2)`) and the
/// stringize-comparison idiom FreeBASIC's own docs show for a variadic
/// macro parameter (`#if #arg2 = ""`).
///
/// Grammar (lowest to highest precedence):
///   expr       := orExpr
///   orExpr     := andExpr ( "or" andExpr )*
///   andExpr    := notExpr ( "and" notExpr )*
///   notExpr    := "not" notExpr | comparison
///   comparison := additive ( ("="|"<>"|"<="|">="|"<"|">") additive )?
///   additive   := term ( ("+"|"-") term )*
///   term       := unary ( ("*"|"/"|"mod") unary )*
///   unary      := "-" unary | primary
///   primary    := integer-literal | string-literal | "(" expr ")"
///
/// String operands only support "=" and "<>" (equality), not ordering or
/// arithmetic - a deliberate scoped simplification over real FreeBASIC,
/// documented in docs/reference/preprocessor.md.
///
/// Returns the integer result (non-zero is true, matching `#if`'s own
/// "non-zero" rule - a bare string result is truthy iff non-empty), or
/// nullopt (with a diagnostic already reported via `report`) on a
/// malformed or type-mismatched expression.
std::optional<long long> evalPreprocessorExpr(const std::string& expr,
                                               const std::function<void(const std::string&)>& report);

} // namespace ebasic
