#include "sema/sema.hpp"

#include <cctype>

namespace ebasic {

std::string Sema::canonicalName(const std::string& name) {
    std::string r = name;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

namespace {
bool isCaseCompatible(TypeKind a, TypeKind b) {
    return (isNumericType(a) && isNumericType(b)) || (a == TypeKind::StringT && b == TypeKind::StringT);
}
} // namespace

void Sema::check(Module& module) {
    collectLabels(module.stmts);
    checkBlock(module.stmts, /*atTopLevel=*/true);
}

void Sema::collectLabels(std::vector<StmtPtr>& stmts) {
    for (auto& stmt : stmts) {
        if (stmt->kind != StmtKind::Label) continue;
        std::string key = canonicalName(stmt->name);
        if (!labels_.insert(key).second) {
            diags_.error(stmt->loc, "label '" + stmt->name + "' is already declared");
        }
    }
}

void Sema::checkBlock(std::vector<StmtPtr>& stmts, bool atTopLevel) {
    for (auto& stmt : stmts) {
        checkStmt(*stmt, atTopLevel);
    }
}

void Sema::checkCondition(Expr& expr, const char* what) {
    TypeKind t = checkExpr(expr);
    if (!isNumericType(t)) {
        diags_.error(expr.loc, std::string(what) + " must be a numeric expression");
    }
}

void Sema::checkStmt(Stmt& stmt, bool atTopLevel) {
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
        case StmtKind::If: {
            for (auto& cond : stmt.conditions) {
                checkCondition(*cond, "IF/ELSEIF condition");
            }
            for (auto& block : stmt.blocks) {
                checkBlock(block, /*atTopLevel=*/false);
            }
            return;
        }
        case StmtKind::SelectCase: {
            TypeKind selectorType = checkExpr(*stmt.expr);
            for (size_t i = 0; i < stmt.cases.size(); ++i) {
                CaseArm& arm = stmt.cases[i];
                if (arm.isElse && i + 1 != stmt.cases.size()) {
                    diags_.error(stmt.loc, "CASE ELSE must be the last CASE in a SELECT CASE");
                }
                for (auto& match : arm.matches) {
                    TypeKind matchType = checkExpr(*match);
                    if (!isCaseCompatible(selectorType, matchType)) {
                        diags_.error(match->loc,
                                     "CASE value is not comparable to the SELECT CASE expression");
                    }
                }
                checkBlock(arm.body, /*atTopLevel=*/false);
            }
            return;
        }
        case StmtKind::ForNext: {
            std::string key = canonicalName(stmt.name);
            auto it = symbols_.find(key);
            if (it == symbols_.end()) {
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is not declared");
            } else if (!isNumericType(it->second)) {
                diags_.error(stmt.loc, "FOR loop variable '" + stmt.name + "' must be numeric");
            }
            checkCondition(*stmt.expr, "FOR start value");
            checkCondition(*stmt.forEnd, "FOR end value (TO)");
            if (stmt.forStep) checkCondition(*stmt.forStep, "FOR step value");
            loopStack_.push_back(LoopKind::For);
            checkBlock(stmt.body, /*atTopLevel=*/false);
            loopStack_.pop_back();
            return;
        }
        case StmtKind::DoLoop: {
            if (stmt.preTest != LoopTest::None) checkCondition(*stmt.preCond, "DO condition");
            loopStack_.push_back(LoopKind::Do);
            checkBlock(stmt.body, /*atTopLevel=*/false);
            loopStack_.pop_back();
            if (stmt.postTest != LoopTest::None) checkCondition(*stmt.postCond, "LOOP condition");
            return;
        }
        case StmtKind::WhileWend: {
            checkCondition(*stmt.expr, "WHILE condition");
            loopStack_.push_back(LoopKind::While);
            checkBlock(stmt.body, /*atTopLevel=*/false);
            loopStack_.pop_back();
            return;
        }
        case StmtKind::Goto: {
            if (!atTopLevel) {
                diags_.error(stmt.loc,
                             "GOTO is only supported at the top level of a program in this "
                             "version of ebc");
            }
            if (!labels_.count(canonicalName(stmt.name))) {
                diags_.error(stmt.loc, "label '" + stmt.name + "' is not defined");
            }
            return;
        }
        case StmtKind::Label: {
            if (!atTopLevel) {
                diags_.error(stmt.loc,
                             "labels are only supported at the top level of a program in this "
                             "version of ebc");
            }
            return;
        }
        case StmtKind::ExitLoop: {
            bool found = false;
            for (auto it = loopStack_.rbegin(); it != loopStack_.rend(); ++it) {
                if (*it == stmt.exitKind) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                const char* kind = stmt.exitKind == LoopKind::For   ? "FOR"
                                   : stmt.exitKind == LoopKind::Do  ? "DO"
                                                                    : "WHILE";
                diags_.error(stmt.loc,
                             std::string("EXIT ") + kind + " used outside of a matching loop");
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
        case ExprKind::BoolLiteral:
            expr.type = TypeKind::Boolean;
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
            if (!isNumericType(t)) {
                diags_.error(expr.loc, "unary '-' requires a numeric operand");
                expr.type = TypeKind::Integer;
                return expr.type;
            }
            expr.type = t;
            return expr.type;
        }
        case ExprKind::UnaryNot: {
            TypeKind t = checkExpr(*expr.lhs);
            if (!isIntegerFamily(t)) {
                diags_.error(expr.loc, "'NOT' requires an integer or boolean operand");
                expr.type = TypeKind::Integer;
                return expr.type;
            }
            expr.type = t;
            return expr.type;
        }
        case ExprKind::Binary: {
            TypeKind lt = checkExpr(*expr.lhs);
            TypeKind rt = checkExpr(*expr.rhs);

            switch (expr.binOp) {
                case BinOp::Concat:
                    if (lt != TypeKind::StringT || rt != TypeKind::StringT) {
                        diags_.error(expr.loc, "operator '&' requires string operands");
                    }
                    expr.type = TypeKind::StringT;
                    return expr.type;

                case BinOp::Add:
                case BinOp::Sub:
                case BinOp::Mul:
                    if (!isNumericType(lt) || !isNumericType(rt)) {
                        diags_.error(expr.loc, "arithmetic operators require numeric operands");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = promoteNumeric(lt, rt);
                    return expr.type;

                case BinOp::Div:
                    if (!isNumericType(lt) || !isNumericType(rt)) {
                        diags_.error(expr.loc, "arithmetic operators require numeric operands");
                        expr.type = TypeKind::Double;
                        return expr.type;
                    }
                    // '/' is always real division in FreeBASIC, even between
                    // two integer operands (use '\' for integer division).
                    expr.type = (!isFloatFamily(lt) && !isFloatFamily(rt))
                                    ? TypeKind::Double
                                    : promoteNumeric(lt, rt);
                    return expr.type;

                case BinOp::Pow:
                    if (!isNumericType(lt) || !isNumericType(rt)) {
                        diags_.error(expr.loc, "'^' requires numeric operands");
                    }
                    // Exponentiation always yields a real result here; FB's
                    // integer-power special case is deferred (see roadmap).
                    expr.type = TypeKind::Double;
                    return expr.type;

                case BinOp::IDiv:
                case BinOp::Mod:
                    if (!isIntegerFamily(lt) || !isIntegerFamily(rt)) {
                        diags_.error(expr.loc,
                                     "'\\' and 'MOD' require integer operands "
                                     "(convert floating-point operands explicitly)");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = promoteInteger(lt, rt);
                    return expr.type;

                case BinOp::Shl:
                case BinOp::Shr:
                    if (!isIntegerFamily(lt) || !isIntegerFamily(rt)) {
                        diags_.error(expr.loc, "'SHL' and 'SHR' require integer operands");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = lt; // shift result takes the shifted value's type
                    return expr.type;

                case BinOp::Eq:
                case BinOp::Ne:
                case BinOp::Lt:
                case BinOp::Le:
                case BinOp::Gt:
                case BinOp::Ge:
                    if (!((isNumericType(lt) && isNumericType(rt)) ||
                          (lt == TypeKind::StringT && rt == TypeKind::StringT))) {
                        diags_.error(expr.loc,
                                     "relational operators require two numeric operands or two "
                                     "string operands");
                    }
                    expr.type = TypeKind::Boolean;
                    return expr.type;

                case BinOp::And:
                case BinOp::Or:
                case BinOp::Xor:
                    if (!isIntegerFamily(lt) || !isIntegerFamily(rt)) {
                        diags_.error(expr.loc,
                                     "'AND'/'OR'/'XOR' require integer or boolean operands");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = promoteInteger(lt, rt);
                    return expr.type;
            }
            expr.type = TypeKind::Unknown;
            return expr.type;
        }
    }
    expr.type = TypeKind::Unknown;
    return expr.type;
}

} // namespace ebasic
