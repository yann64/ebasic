#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostics.hpp"

#include <string>
#include <unordered_map>

namespace ebasic {

class Sema {
public:
    explicit Sema(DiagnosticEngine& diags) : diags_(diags) {}

    void check(Module& module);

private:
    void checkStmt(Stmt& stmt);
    TypeKind checkExpr(Expr& expr);
    static std::string canonicalName(const std::string& name);

    std::unordered_map<std::string, TypeKind> symbols_;
    DiagnosticEngine& diags_;
};

} // namespace ebasic
