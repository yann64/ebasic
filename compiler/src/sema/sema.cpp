#include "sema/sema.hpp"

#include <cctype>

namespace ebasic {

namespace {
bool isNumeric(TypeKind t) { return t == TypeKind::Integer || t == TypeKind::Double; }
} // namespace

std::string Sema::canonicalName(const std::string& name) {
    std::string r = name;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

void Sema::check(Module& module) {
    for (auto& stmt : module.stmts) {
        checkStmt(*stmt);
    }
}

void Sema::checkStmt(Stmt& stmt) {
    switch (stmt.kind) {
        case StmtKind::Dim: {
            std::string key = canonicalName(stmt.name);
            if (symbols_.count(key)) {
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is already declared");
                return;
            }
            symbols_[key] = stmt.declaredType;
            return;
        }
        case StmtKind::Assign: {
            std::string key = canonicalName(stmt.name);
            auto it = symbols_.find(key);
            if (it == symbols_.end()) {
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is not declared");
                return;
            }
            TypeKind exprType = checkExpr(*stmt.expr);
            TypeKind varType = it->second;
            if (varType == TypeKind::StringT && exprType != TypeKind::StringT) {
                diags_.error(stmt.loc,
                             "cannot assign a non-string value to string variable '" + stmt.name + "'");
            } else if (varType != TypeKind::StringT && exprType == TypeKind::StringT) {
                diags_.error(stmt.loc,
                             "cannot assign a string value to numeric variable '" + stmt.name + "'");
            }
            return;
        }
        case StmtKind::Print: {
            for (auto& arg : stmt.args) {
                checkExpr(*arg);
            }
            return;
        }
    }
}

TypeKind Sema::checkExpr(Expr& expr) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
            expr.type = TypeKind::Integer;
            return expr.type;
        case ExprKind::DoubleLiteral:
            expr.type = TypeKind::Double;
            return expr.type;
        case ExprKind::StringLiteral:
            expr.type = TypeKind::StringT;
            return expr.type;
        case ExprKind::Ident: {
            std::string key = canonicalName(expr.stringValue);
            auto it = symbols_.find(key);
            if (it == symbols_.end()) {
                diags_.error(expr.loc, "variable '" + expr.stringValue + "' is not declared");
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            expr.type = it->second;
            return expr.type;
        }
        case ExprKind::UnaryNeg: {
            TypeKind t = checkExpr(*expr.lhs);
            if (!isNumeric(t)) {
                diags_.error(expr.loc, "unary '-' requires a numeric operand");
                expr.type = TypeKind::Integer;
                return expr.type;
            }
            expr.type = t;
            return expr.type;
        }
        case ExprKind::Binary: {
            TypeKind lt = checkExpr(*expr.lhs);
            TypeKind rt = checkExpr(*expr.rhs);
            if (expr.binOp == BinOp::Concat) {
                if (lt != TypeKind::StringT || rt != TypeKind::StringT) {
                    diags_.error(expr.loc, "operator '&' requires string operands");
                }
                expr.type = TypeKind::StringT;
                return expr.type;
            }
            if (!isNumeric(lt) || !isNumeric(rt)) {
                diags_.error(expr.loc, "arithmetic operators require numeric operands");
                expr.type = TypeKind::Integer;
                return expr.type;
            }
            expr.type = (lt == TypeKind::Double || rt == TypeKind::Double) ? TypeKind::Double
                                                                            : TypeKind::Integer;
            return expr.type;
        }
    }
    expr.type = TypeKind::Unknown;
    return expr.type;
}

} // namespace ebasic
