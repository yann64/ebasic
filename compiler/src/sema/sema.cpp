#include "sema/sema.hpp"

#include <algorithm>
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
    collectProcedures(module.stmts);
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

void Sema::collectProcedures(std::vector<StmtPtr>& stmts) {
    for (auto& stmt : stmts) {
        if (stmt->kind != StmtKind::SubDecl && stmt->kind != StmtKind::FunctionDecl) continue;
        std::string key = canonicalName(stmt->name);
        if (symbols_.count(key) || procedures_.count(key)) {
            diags_.error(stmt->loc, "'" + stmt->name + "' is already declared");
            continue;
        }
        ProcedureInfo info;
        info.isFunction = stmt->kind == StmtKind::FunctionDecl;
        info.returnType = stmt->declaredType;
        info.params = stmt->params;
        procedures_[key] = std::move(info);
    }
}

bool Sema::lookupSymbol(const std::string& key, SymbolInfo& out) const {
    auto lit = locals_.find(key);
    if (lit != locals_.end()) {
        out = lit->second;
        return true;
    }
    auto git = symbols_.find(key);
    if (git != symbols_.end()) {
        out = git->second;
        return true;
    }
    return false;
}

void Sema::checkCallArgs(const ProcedureInfo& proc, std::vector<ExprPtr>& args, SourceLoc loc) {
    if (args.size() != proc.params.size()) {
        diags_.error(loc, "expected " + std::to_string(proc.params.size()) + " argument(s), got " +
                               std::to_string(args.size()));
    }
    size_t n = std::min(args.size(), proc.params.size());
    for (size_t i = 0; i < n; ++i) {
        const Param& param = proc.params[i];
        Expr& arg = *args[i];
        TypeKind argType = checkExpr(arg);
        if (!isAssignCompatible(param.type, argType)) {
            diags_.error(arg.loc, "argument " + std::to_string(i + 1) +
                                       " type does not match parameter '" + param.name + "'");
        }
        if (param.byRef) {
            bool isPlainVar = arg.kind == ExprKind::Ident;
            SymbolInfo info;
            bool isConstVar = isPlainVar && lookupSymbol(canonicalName(arg.stringValue), info) &&
                               info.isConst;
            if (!isPlainVar || isConstVar) {
                diags_.error(arg.loc, "argument " + std::to_string(i + 1) + " passed to BYREF "
                                       "parameter '" + param.name + "' must be a plain variable");
            }
        }
    }
    // Any extra args beyond the parameter count still get type-checked so
    // their own errors (undeclared names, etc.) aren't silently skipped.
    for (size_t i = n; i < args.size(); ++i) {
        checkExpr(*args[i]);
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
            auto& scope = insideProcedure_ ? locals_ : symbols_;
            if (scope.count(key) || procedures_.count(key)) {
                diags_.error(stmt.loc, "'" + stmt.name + "' is already declared");
                return;
            }
            if (stmt.isArray) {
                if (stmt.arrayLower && !isIntegerFamily(checkExpr(*stmt.arrayLower))) {
                    diags_.error(stmt.arrayLower->loc, "array lower bound must be an integer expression");
                }
                if (stmt.arrayUpper && !isIntegerFamily(checkExpr(*stmt.arrayUpper))) {
                    diags_.error(stmt.arrayUpper->loc, "array upper bound must be an integer expression");
                }
            }
            SymbolInfo info;
            info.type = stmt.declaredType;
            info.isConst = false;
            info.isArray = stmt.isArray;
            info.isDynamicArray = stmt.isArray && !stmt.arrayUpper;
            scope[key] = info;
            return;
        }
        case StmtKind::Redim: {
            std::string key = canonicalName(stmt.name);
            SymbolInfo info;
            if (!lookupSymbol(key, info)) {
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is not declared");
                return;
            }
            if (!info.isArray) {
                diags_.error(stmt.loc, "'" + stmt.name + "' is not an array");
                return;
            }
            if (!info.isDynamicArray) {
                diags_.error(stmt.loc,
                             "'" + stmt.name + "' is a fixed-size array and cannot be REDIM'd "
                             "(declare it with DIM " + stmt.name + "() to make it dynamic)");
            }
            if (stmt.declaredType != TypeKind::Unknown && stmt.declaredType != info.type) {
                diags_.error(stmt.loc, "REDIM type does not match the array's declared type");
            }
            if (stmt.arrayLower && !isIntegerFamily(checkExpr(*stmt.arrayLower))) {
                diags_.error(stmt.arrayLower->loc, "array lower bound must be an integer expression");
            }
            if (!isIntegerFamily(checkExpr(*stmt.arrayUpper))) {
                diags_.error(stmt.arrayUpper->loc, "array upper bound must be an integer expression");
            }
            return;
        }
        case StmtKind::Const: {
            std::string key = canonicalName(stmt.name);
            auto& scope = insideProcedure_ ? locals_ : symbols_;
            if (scope.count(key) || procedures_.count(key)) {
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
            scope[key] = SymbolInfo{constType, /*isConst=*/true, false};
            return;
        }
        case StmtKind::Enum: {
            auto& scope = insideProcedure_ ? locals_ : symbols_;
            long long next = 0;
            for (auto& member : stmt.enumMembers) {
                long long value = next;
                if (member.value) {
                    long long v = 0;
                    if (evalConstInt(*member.value, v)) value = v;
                }
                member.resolvedValue = value;
                std::string key = canonicalName(member.name);
                if (scope.count(key) || procedures_.count(key)) {
                    diags_.error(member.loc, "'" + member.name + "' is already declared");
                } else {
                    scope[key] = SymbolInfo{TypeKind::Integer, /*isConst=*/true, false};
                    constIntValues_[key] = value;
                }
                next = value + 1;
            }
            return;
        }
        case StmtKind::Assign: {
            if (stmt.isReturnAssign) {
                TypeKind exprType = checkExpr(*stmt.expr);
                if (!isAssignCompatible(currentFunctionReturnType_, exprType)) {
                    diags_.error(stmt.loc, "return value type does not match FUNCTION '" + stmt.name +
                                               "'s declared return type");
                }
                return;
            }
            std::string key = canonicalName(stmt.name);
            SymbolInfo info;
            if (!lookupSymbol(key, info)) {
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is not declared");
                return;
            }
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
            SymbolInfo info;
            if (!lookupSymbol(key, info)) {
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is not declared");
            } else if (info.isConst) {
                diags_.error(stmt.loc, "FOR loop variable '" + stmt.name + "' cannot be a constant");
            } else if (info.isArray) {
                diags_.error(stmt.loc, "FOR loop variable '" + stmt.name + "' cannot be an array");
            } else if (!isNumericType(info.type)) {
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
                const char* kind = stmt.exitKind == LoopKind::For        ? "FOR"
                                   : stmt.exitKind == LoopKind::Do       ? "DO"
                                   : stmt.exitKind == LoopKind::While    ? "WHILE"
                                   : stmt.exitKind == LoopKind::Sub      ? "SUB"
                                                                         : "FUNCTION";
                diags_.error(stmt.loc,
                             std::string("EXIT ") + kind + " used outside of a matching " + kind);
            }
            return;
        }
        case StmtKind::SubDecl:
        case StmtKind::FunctionDecl: {
            if (!atTopLevel) {
                diags_.error(stmt.loc,
                             "SUB/FUNCTION declarations are only supported at the top level of a "
                             "program");
                return;
            }
            bool isFunction = stmt.kind == StmtKind::FunctionDecl;

            std::unordered_map<std::string, SymbolInfo> savedLocals = std::move(locals_);
            bool savedInsideProcedure = insideProcedure_;
            TypeKind savedReturnType = currentFunctionReturnType_;

            locals_.clear();
            insideProcedure_ = true;
            currentFunctionReturnType_ = isFunction ? stmt.declaredType : TypeKind::Unknown;

            for (const Param& param : stmt.params) {
                std::string key = canonicalName(param.name);
                if (locals_.count(key)) {
                    diags_.error(param.loc, "duplicate parameter name '" + param.name + "'");
                    continue;
                }
                locals_[key] = SymbolInfo{param.type, /*isConst=*/false, /*isArray=*/false};
            }

            loopStack_.push_back(isFunction ? LoopKind::Function : LoopKind::Sub);
            checkBlock(stmt.body, /*atTopLevel=*/false);
            loopStack_.pop_back();

            locals_ = std::move(savedLocals);
            insideProcedure_ = savedInsideProcedure;
            currentFunctionReturnType_ = savedReturnType;
            return;
        }
        case StmtKind::CallStmt: {
            std::string key = canonicalName(stmt.name);
            auto procIt = procedures_.find(key);
            if (procIt == procedures_.end()) {
                diags_.error(stmt.loc, "'" + stmt.name + "' is not a declared SUB or FUNCTION");
                for (auto& arg : stmt.args) checkExpr(*arg);
                return;
            }
            checkCallArgs(procIt->second, stmt.args, stmt.loc);
            return;
        }
        case StmtKind::Return: {
            bool insideSub = false;
            bool insideFunction = false;
            for (auto it = loopStack_.rbegin(); it != loopStack_.rend(); ++it) {
                if (*it == LoopKind::Sub) { insideSub = true; break; }
                if (*it == LoopKind::Function) { insideFunction = true; break; }
            }
            if (!insideSub && !insideFunction) {
                diags_.error(stmt.loc, "RETURN used outside of a SUB or FUNCTION");
                if (stmt.expr) checkExpr(*stmt.expr);
                return;
            }
            if (insideFunction) {
                if (!stmt.expr) {
                    diags_.error(stmt.loc, "RETURN inside a FUNCTION requires a value");
                    return;
                }
                TypeKind exprType = checkExpr(*stmt.expr);
                if (!isAssignCompatible(currentFunctionReturnType_, exprType)) {
                    diags_.error(stmt.expr->loc,
                                 "RETURN value type does not match the FUNCTION's declared return "
                                 "type");
                }
            } else if (stmt.expr) {
                diags_.error(stmt.expr->loc, "SUB cannot RETURN a value; use a bare RETURN or EXIT SUB");
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
            SymbolInfo info;
            if (!lookupSymbol(key, info)) {
                diags_.error(expr.loc, "variable '" + expr.stringValue + "' is not declared");
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            if (info.isArray) {
                diags_.error(expr.loc, "array '" + expr.stringValue + "' must be indexed, e.g. " +
                                           expr.stringValue + "(i)");
            }
            expr.type = info.type;
            return expr.type;
        }
        case ExprKind::Call: {
            std::string key = canonicalName(expr.stringValue);
            SymbolInfo info;
            bool isVar = lookupSymbol(key, info);
            if (isVar && info.isArray) {
                if (expr.args.size() != 1) {
                    diags_.error(expr.loc,
                                 "array '" + expr.stringValue + "' takes exactly one index");
                }
                for (auto& arg : expr.args) {
                    if (!isIntegerFamily(checkExpr(*arg))) {
                        diags_.error(arg->loc, "array index must be an integer expression");
                    }
                }
                expr.type = info.type;
                return expr.type;
            }
            auto procIt = procedures_.find(key);
            if (procIt != procedures_.end()) {
                const ProcedureInfo& proc = procIt->second;
                if (!proc.isFunction) {
                    diags_.error(expr.loc, "'" + expr.stringValue +
                                                "' is a SUB and cannot be used in an expression");
                }
                checkCallArgs(proc, expr.args, expr.loc);
                expr.type = proc.returnType;
                return expr.type;
            }
            if (isVar) {
                diags_.error(expr.loc, "'" + expr.stringValue + "' is not an array or function");
            } else {
                diags_.error(expr.loc, "'" + expr.stringValue + "' is not declared");
            }
            for (auto& arg : expr.args) checkExpr(*arg);
            expr.type = TypeKind::Unknown;
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
            SymbolInfo info;
            return lookupSymbol(canonicalName(expr.stringValue), info) && info.isConst;
        }
        case ExprKind::Call:
            return false; // array elements and function calls are never compile-time constants here
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
