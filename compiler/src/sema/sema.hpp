#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostics.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ebasic {

// A declared name: its element type (for arrays, the element's type), and
// whether it's an array and/or immutable (CONST/ENUM member).
struct SymbolInfo {
    TypeKind type = TypeKind::Unknown;
    bool isConst = false;
    bool isArray = false;
};

class Sema {
public:
    explicit Sema(DiagnosticEngine& diags) : diags_(diags) {}

    void check(Module& module);

private:
    void collectLabels(std::vector<StmtPtr>& stmts);
    void checkBlock(std::vector<StmtPtr>& stmts, bool atTopLevel);
    void checkStmt(Stmt& stmt, bool atTopLevel);
    void checkCondition(Expr& expr, const char* what);
    TypeKind checkExpr(Expr& expr);

    // Structural constant-expression check for CONST initializers: literals,
    // and Idents that refer to an already-declared CONST/ENUM member,
    // combined with unary/binary operators. Does not compute a value -
    // codegen re-emits the original expression and lets C++ evaluate it.
    bool isConstantExpr(const Expr& expr) const;

    // Small integer-only constant evaluator, used only for ENUM member
    // values (needed for auto-increment). Reports its own diagnostics and
    // returns false on failure.
    bool evalConstInt(const Expr& expr, long long& outValue);

    std::unordered_map<std::string, SymbolInfo> symbols_;
    std::unordered_map<std::string, long long> constIntValues_; // CONST/ENUM int value, for evalConstInt
    std::unordered_set<std::string> labels_;
    std::vector<LoopKind> loopStack_;
    DiagnosticEngine& diags_;
};

} // namespace ebasic
