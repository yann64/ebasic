#include "sema/sema.hpp"

#include <cctype>

namespace ebasic {

namespace {
bool isCaseCompatible(TypeKind a, TypeKind b) {
    return (isNumericType(a) && isNumericType(b)) || (a == TypeKind::StringT && b == TypeKind::StringT);
}

// Checks a value's type against a target variable/const type using the same
// string-vs-numeric compatibility rule used throughout (no narrower checks).
bool isAssignCompatible(TypeKind targetType, TypeKind valueType) {
    bool targetIsString = targetType == TypeKind::StringT;
    bool valueIsString = valueType == TypeKind::StringT;
    return targetIsString == valueIsString;
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
            if (stmt.isArray) {
                if (stmt.arrayLower && !isIntegerFamily(checkExpr(*stmt.arrayLower))) {
                    diags_.error(stmt.arrayLower->loc, "array lower bound must be an integer expression");
                }
                if (!isIntegerFamily(checkExpr(*stmt.arrayUpper))) {
                    diags_.error(stmt.arrayUpper->loc, "array upper bound must be an integer expression");
                }
            }
            symbols_[key] = SymbolInfo{stmt.declaredType, /*isConst=*/false, stmt.isArray};
            return;
        }
        case StmtKind::Const: {
            std::string key = canonicalName(stmt.name);
            if (symbols_.count(key)) {
                diags_.error(stmt.loc, "'" + stmt.name + "' is already declared");
                return;
            }
            TypeKind exprType = checkExpr(*stmt.expr);
            TypeKind constType = (stmt.declaredType != TypeKind::Unknown) ? stmt.declaredType : exprType;
            stmt.declaredType = constType; // resolve for codegen, even when inferred
            if (!isAssignCompatible(constType, exprType)) {
                diags_.error(stmt.loc, "CONST '" + stmt.name + "' initializer type does not match its "
                                       "declared type");
            }
            if (!isConstantExpr(*stmt.expr)) {
                diags_.error(stmt.expr->loc,
                             "CONST initializer must be a constant expression (literals and "
                             "other CONST/ENUM names only)");
            }
            symbols_[key] = SymbolInfo{constType, /*isConst=*/true, false};
            return;
        }
        case StmtKind::Enum: {
            long long next = 0;
            for (auto& member : stmt.enumMembers) {
                long long value = next;
                if (member.value) {
                    long long v = 0;
                    if (evalConstInt(*member.value, v)) value = v;
                }
                member.resolvedValue = value;
                std::string key = canonicalName(member.name);
                if (symbols_.count(key)) {
                    diags_.error(member.loc, "'" + member.name + "' is already declared");
                } else {
                    symbols_[key] = SymbolInfo{TypeKind::Integer, /*isConst=*/true, false};
                    constIntValues_[key] = value;
                }
                next = value + 1;
            }
            return;
        }
        case StmtKind::Assign: {
            std::string key = canonicalName(stmt.name);
            auto it = symbols_.find(key);
            if (it == symbols_.end()) {
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is not declared");
                return;
            }
            const SymbolInfo& info = it->second;
            if (info.isConst) {
                diags_.error(stmt.loc, "cannot assign to constant '" + stmt.name + "'");
                return;
            }
            if (stmt.index) {
                if (!info.isArray) {
                    diags_.error(stmt.loc, "'" + stmt.name + "' is not an array");
                }
                if (!isIntegerFamily(checkExpr(*stmt.index))) {
                    diags_.error(stmt.index->loc, "array index must be an integer expression");
                }
            } else if (info.isArray) {
                diags_.error(stmt.loc, "array '" + stmt.name + "' must be indexed, e.g. " + stmt.name +
                                           "(i) = ...");
            }
            TypeKind exprType = checkExpr(*stmt.expr);
            TypeKind varType = info.type;
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
            } else if (it->second.isConst) {
                diags_.error(stmt.loc, "FOR loop variable '" + stmt.name + "' cannot be a constant");
            } else if (it->second.isArray) {
                diags_.error(stmt.loc, "FOR loop variable '" + stmt.name + "' cannot be an array");
            } else if (!isNumericType(it->second.type)) {
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
            if (it->second.isArray) {
                diags_.error(expr.loc, "array '" + expr.stringValue + "' must be indexed, e.g. " +
                                           expr.stringValue + "(i)");
            }
            expr.type = it->second.type;
            return expr.type;
        }
        case ExprKind::Index: {
            std::string key = canonicalName(expr.stringValue);
            auto it = symbols_.find(key);
            if (it == symbols_.end()) {
                diags_.error(expr.loc, "variable '" + expr.stringValue + "' is not declared");
                checkExpr(*expr.rhs);
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            if (!it->second.isArray) {
                diags_.error(expr.loc, "'" + expr.stringValue + "' is not an array");
            }
            if (!isIntegerFamily(checkExpr(*expr.rhs))) {
                diags_.error(expr.rhs->loc, "array index must be an integer expression");
            }
            expr.type = it->second.type;
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

bool Sema::isConstantExpr(const Expr& expr) const {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
        case ExprKind::DoubleLiteral:
        case ExprKind::StringLiteral:
        case ExprKind::BoolLiteral:
            return true;
        case ExprKind::Ident: {
            auto it = symbols_.find(canonicalName(expr.stringValue));
            return it != symbols_.end() && it->second.isConst;
        }
        case ExprKind::Index:
            return false; // array elements are never a compile-time constant here
        case ExprKind::UnaryNeg:
        case ExprKind::UnaryNot:
            return isConstantExpr(*expr.lhs);
        case ExprKind::Binary:
            return isConstantExpr(*expr.lhs) && isConstantExpr(*expr.rhs);
    }
    return false;
}

bool Sema::evalConstInt(const Expr& expr, long long& outValue) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
        case ExprKind::BoolLiteral:
            outValue = expr.intValue;
            return true;
        case ExprKind::Ident: {
            auto it = constIntValues_.find(canonicalName(expr.stringValue));
            if (it == constIntValues_.end()) {
                diags_.error(expr.loc,
                             "'" + expr.stringValue + "' is not a constant integer expression");
                return false;
            }
            outValue = it->second;
            return true;
        }
        case ExprKind::UnaryNeg: {
            long long v = 0;
            if (!evalConstInt(*expr.lhs, v)) return false;
            outValue = -v;
            return true;
        }
        case ExprKind::UnaryNot: {
            long long v = 0;
            if (!evalConstInt(*expr.lhs, v)) return false;
            outValue = ~v;
            return true;
        }
        case ExprKind::Binary: {
            long long l = 0, r = 0;
            if (!evalConstInt(*expr.lhs, l) || !evalConstInt(*expr.rhs, r)) return false;
            switch (expr.binOp) {
                case BinOp::Add: outValue = l + r; return true;
                case BinOp::Sub: outValue = l - r; return true;
                case BinOp::Mul: outValue = l * r; return true;
                case BinOp::IDiv:
                case BinOp::Div:
                    if (r == 0) {
                        diags_.error(expr.loc, "division by zero in constant expression");
                        return false;
                    }
                    outValue = l / r;
                    return true;
                case BinOp::Mod:
                    if (r == 0) {
                        diags_.error(expr.loc, "division by zero in constant expression");
                        return false;
                    }
                    outValue = l % r;
                    return true;
                case BinOp::Shl: outValue = l << r; return true;
                case BinOp::Shr: outValue = l >> r; return true;
                case BinOp::And: outValue = l & r; return true;
                case BinOp::Or: outValue = l | r; return true;
                case BinOp::Xor: outValue = l ^ r; return true;
                default:
                    diags_.error(expr.loc, "expression is not a valid constant integer expression");
                    return false;
            }
        }
        default:
            diags_.error(expr.loc, "expression is not a valid constant integer expression");
            return false;
    }
}

} // namespace ebasic
