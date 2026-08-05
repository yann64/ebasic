#include "codegen/codegen.hpp"

#include <cctype>
#include <functional>
#include <optional>
#include <stdexcept>

namespace ebasic {

std::string Codegen::mangleName(const std::string& name) {
    std::string r = "eb_";
    for (char c : name) r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return r;
}

std::string Codegen::gosubFunctionName(const std::string& label) {
    return "eb_gosub_" + canonicalName(label);
}

std::string Codegen::cppType(const Type& type) {
    switch (type.kind) {
        case TypeKind::Byte: return "std::int8_t";
        case TypeKind::UByte: return "std::uint8_t";
        case TypeKind::Short: return "std::int16_t";
        case TypeKind::UShort: return "std::uint16_t";
        case TypeKind::Integer: return "std::int32_t";
        case TypeKind::Long: return "std::int32_t";
        case TypeKind::UInteger: return "std::uint32_t";
        case TypeKind::LongInt: return "std::int64_t";
        case TypeKind::ULongInt: return "std::uint64_t";
        case TypeKind::Single: return "float";
        case TypeKind::Double: return "double";
        case TypeKind::Boolean: return "std::int8_t";
        case TypeKind::StringT: return "::ebasic::rt::BString";
        case TypeKind::ZStringT: return "const char*";
        /// A TYPE and a variable can never share a name (Sema shares one
        /// namespace for both), so reusing mangleName for the struct's own
        /// name can't collide with any variable's mangled name.
        case TypeKind::UserDefined: return mangleName(type.typeName);
        case TypeKind::Pointer:
            /// ANY PTR (null pointee) is FB's void*-equivalent.
            return type.pointee ? (cppType(*type.pointee) + "*") : "void*";
        case TypeKind::Unknown: break;
    }
    throw std::runtime_error("codegen: unresolved type reached codegen");
}

std::string Codegen::escapeStringLiteral(const std::string& s) {
    std::string out;
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            default: out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

namespace {
/// Recursively evaluates a compile-time-constant literal expression for
/// generateLibraryInterface's CONST re-export (see its own doc comment) -
/// a bare IntLiteral/DoubleLiteral, or one negated any number of times
/// (`-3`, `--3`, ... - UnaryNeg is right-associative, see Parser::
/// parseNegate) - returning its literal text with the correct sign, or
/// std::nullopt if `expr` isn't one of these (some other, more general
/// constant expression this function deliberately doesn't try to
/// re-derive, to avoid silently emitting the wrong value). Found needed
/// when a real negative-valued C enum constant (GTK_RESPONSE_ACCEPT = -3)
/// was silently unexported - only a bare, non-negated literal was
/// recognized before this.
std::optional<std::string> tryEvalConstLiteralText(const Expr& expr, bool negate) {
    if (expr.kind == ExprKind::IntLiteral) {
        return std::to_string(negate ? -expr.intValue : expr.intValue);
    }
    if (expr.kind == ExprKind::DoubleLiteral) {
        return std::to_string(negate ? -expr.doubleValue : expr.doubleValue);
    }
    if (expr.kind == ExprKind::UnaryNeg && expr.lhs) {
        return tryEvalConstLiteralText(*expr.lhs, !negate);
    }
    return std::nullopt;
}

/// True for the six comparison operators - genExpr uses this to render their
/// result as BASIC's own -1/0 boolean convention (via a ternary + explicit
/// cast) rather than plain C++'s `true`/`false`.
bool isRelational(BinOp op) {
    switch (op) {
        case BinOp::Eq:
        case BinOp::Ne:
        case BinOp::Lt:
        case BinOp::Le:
        case BinOp::Gt:
        case BinOp::Ge:
            return true;
        default:
            return false;
    }
}
} // namespace

std::string Codegen::memberReceiverPrefix(const Expr& lhs) {
    /// Namespace.Name -> C++ eb_ns::eb_name; This -> this->eb_name (a
    /// normal, potentially-virtual access/call); Base -> a *qualified*,
    /// non-virtual access to the immediate base's own implementation
    /// (eb_base::eb_name, implicitly using the current `this` - exactly
    /// like C++'s own `Base::member` from inside a derived member
    /// function); anything else (a variable, a field, ...) is plain
    /// `.`-access on that receiver.
    if (lhs.kind == ExprKind::Ident && namespaces_.count(canonicalName(lhs.stringValue))) {
        return mangleName(lhs.stringValue) + "::";
    }
    if (lhs.kind == ExprKind::This) {
        return "this->";
    }
    if (lhs.kind == ExprKind::Base) {
        return mangleName(baseTypeOf_.at(currentOwnerType_)) + "::";
    }
    return genExpr(lhs) + ".";
}

std::string Codegen::resolveCalleeName(const Expr* lhs, const std::string& name) {
    if (!lhs) {
        auto it = externProcNames_.find(canonicalName(name));
        return it != externProcNames_.end() ? it->second : mangleName(name);
    }
    if (lhs->kind == ExprKind::Ident && namespaces_.count(canonicalName(lhs->stringValue))) {
        std::string key = canonicalName(lhs->stringValue) + "::" + canonicalName(name);
        auto it = externProcNames_.find(key);
        if (it != externProcNames_.end()) return it->second;
        return mangleName(lhs->stringValue) + "::" + mangleName(name);
    }
    return memberReceiverPrefix(*lhs) + mangleName(name);
}

void Codegen::collectExternProcNames(const std::vector<StmtPtr>& stmts, const std::string& keyPrefix,
                                      const std::string& realPrefix) {
    for (const auto& stmtPtr : stmts) {
        if (stmtPtr->kind == StmtKind::NamespaceDecl) {
            /// Two parallel prefixes: `keyPrefix` is built from BASIC-visible
            /// names (what a Call/CallStmt's lhs/target actually spells, used
            /// to look this entry back up), `realPrefix` from the real
            /// (possibly aliased) external name(s) (what Codegen must
            /// actually emit) - they can differ (M4c's whole point).
            std::string nsKey = (keyPrefix.empty() ? "" : keyPrefix + "::") + canonicalName(stmtPtr->name);
            std::string realNs = stmtPtr->externAlias.empty() ? stmtPtr->name : stmtPtr->externAlias;
            std::string nsReal = (realPrefix.empty() ? "" : realPrefix + "::") + realNs;
            collectExternProcNames(stmtPtr->body, nsKey, nsReal);
            continue;
        }
        if (!stmtPtr->isExtern && !stmtPtr->isExported) continue;
        std::string key = (keyPrefix.empty() ? "" : keyPrefix + "::") + canonicalName(stmtPtr->name);
        std::string realName = stmtPtr->externAlias.empty() ? stmtPtr->name : stmtPtr->externAlias;
        externProcNames_[key] = (realPrefix.empty() ? "" : realPrefix + "::") + realName;
    }
}

std::string Codegen::genExpr(const Expr& expr) {
    if (expr.suppressStringWrap) {
        /// See Expr::suppressStringWrap's own doc comment - render the
        /// bare literal (real static storage duration, always safe)
        /// instead of genExprBase's usual BString(...) wrap, which would
        /// otherwise construct a dangling-once-destroyed temporary here.
        return escapeStringLiteral(expr.stringValue);
    }
    std::string result = genExprBase(expr);
    if (expr.pointerCastTo) {
        result = "static_cast<" + cppType(*expr.pointerCastTo) + ">(" + result + ")";
    }
    return result;
}

std::string Codegen::genExprBase(const Expr& expr) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
            return std::to_string(expr.intValue);
        case ExprKind::DoubleLiteral:
            return std::to_string(expr.doubleValue);
        case ExprKind::StringLiteral:
            return "::ebasic::rt::BString(" + escapeStringLiteral(expr.stringValue) + ")";
        case ExprKind::BoolLiteral:
            return std::to_string(expr.intValue);
        case ExprKind::Ident:
            return mangleName(expr.stringValue);
        case ExprKind::Call: {
            if (expr.lhs) {
                /// Namespace.Name(args) -> C++ eb_ns::eb_name(args) (or,
                /// for an Extern "C++" NAMESPACE binding, the real
                /// qualified external name verbatim - see
                /// resolveCalleeName); This -> this->eb_name(args) (a
                /// normal, potentially-virtual call); Base -> a
                /// *qualified*, non-virtual call to the immediate base's
                /// own implementation (eb_base::eb_name(args), implicitly
                /// using the current `this` - exactly like C++'s own
                /// `Base::method()` from inside a derived member function);
                /// anything else (a variable, a field, ...) is a plain
                /// method call on that receiver -> eb_recv.eb_name(args).
                std::string result = resolveCalleeName(expr.lhs.get(), expr.stringValue) + "(";
                for (size_t i = 0; i < expr.args.size(); ++i) {
                    if (i > 0) result += ", ";
                    result += genExpr(*expr.args[i]);
                }
                result += ")";
                return result;
            }
            std::string key = canonicalName(expr.stringValue);
            /// UBound/LBound - Sema already guaranteed the sole argument is
            /// a bare identifier naming an in-scope array, so its own
            /// generated lower-bound variable (arrayLowerBoundVar_) is
            /// guaranteed to exist here. LBound is that variable directly;
            /// UBound adds the vector's current size and subtracts one
            /// (matching real FreeBASIC - an empty array's UBound is
            /// LBound - 1, which this formula already produces for a
            /// zero-size vector).
            if (key == "ubound" || key == "lbound") {
                const std::string& lowerVar =
                    arrayLowerBoundVar_.at(canonicalName(expr.args[0]->stringValue));
                if (key == "lbound") return lowerVar;
                return "static_cast<std::int32_t>(" + lowerVar + " + static_cast<std::int64_t>(" +
                       mangleName(expr.args[0]->stringValue) + ".size()) - 1)";
            }
            auto arrIt = arrayLowerBoundVar_.find(key);
            if (arrIt != arrayLowerBoundVar_.end()) {
                return mangleName(expr.stringValue) + "[static_cast<std::size_t>((" +
                       genExpr(*expr.args[0]) + ") - " + arrIt->second + ")]";
            }
            std::string result = resolveCalleeName(nullptr, expr.stringValue) + "(";
            for (size_t i = 0; i < expr.args.size(); ++i) {
                if (i > 0) result += ", ";
                result += genExpr(*expr.args[i]);
            }
            result += ")";
            return result;
        }
        case ExprKind::Member: {
            std::string prefix = memberReceiverPrefix(*expr.lhs);
            /// A PROPERTY has no native C++ syntax - rewrite the read into a
            /// getter call (`.eb_name_get()`); the write side is handled
            /// separately in Assign's own codegen, since it needs a
            /// completely different shape (`.eb_name_set(value)`, not
            /// `= value`).
            if (expr.isProperty) {
                return prefix + mangleName(expr.stringValue) + "_get()";
            }
            return prefix + mangleName(expr.stringValue);
        }
        case ExprKind::AddressOf:
            /// `@ProcName` (Expr::isProcAddress, set by Sema) - a real C
            /// function pointer converted to `void*` (ANY PTR at the
            /// eBasic level) via an explicit cast, since C++ has no
            /// implicit function-pointer-to-object-pointer conversion
            /// (unlike most other pointer conversions this language
            /// already allows implicitly).
            if (expr.isProcAddress) {
                return "reinterpret_cast<void*>(&" + mangleName(expr.lhs->stringValue) + ")";
            }
            return "(&(" + genExpr(*expr.lhs) + "))";
        case ExprKind::Deref:
            return "(*(" + genExpr(*expr.lhs) + "))";
        case ExprKind::This:
        case ExprKind::Base:
            /// A bare This/Base used as a value (not immediately followed by
            /// `.field`/`.Method()`, which Member/Call special-case above
            /// without going through this branch at all) needs the
            /// dereferenced *value*, not the raw `this` pointer - e.g.
            /// passing `This` to a BYREF/BYVAL parameter, or `@This` taking
            /// its address (`&this` is ill-formed C++; `&(*this)` is not).
            return "(*this)";
        case ExprKind::UnaryNeg:
            return "(-" + genExpr(*expr.lhs) + ")";
        case ExprKind::UnaryNot:
            return "(~(" + genExpr(*expr.lhs) + "))";
        case ExprKind::Binary: {
            const std::string lhs = genExpr(*expr.lhs);
            const std::string rhs = genExpr(*expr.rhs);
            /// A user-defined operand always means a real Operator overload
            /// resolved it (Sema already validated this) - the built-in
            /// special-cased forms below (forced real division, std::pow)
            /// don't apply; a plain textual operator (falling through to the
            /// generic switch) correctly resolves to the user's own
            /// overloaded C++ operator instead.
            bool involvesUserDefined =
                expr.lhs->type.kind == TypeKind::UserDefined || expr.rhs->type.kind == TypeKind::UserDefined;

            if (expr.binOp == BinOp::Div && !involvesUserDefined && !isFloatFamily(expr.lhs->type) &&
                !isFloatFamily(expr.rhs->type)) {
                /// FreeBASIC's '/' is always real division; force it since two
                /// C++ integers would otherwise truncate.
                return "(static_cast<double>(" + lhs + ") / static_cast<double>(" + rhs + "))";
            }
            if (expr.binOp == BinOp::Pow && !involvesUserDefined) {
                return "std::pow(static_cast<double>(" + lhs + "), static_cast<double>(" + rhs +
                       "))";
            }
            if (isRelational(expr.binOp)) {
                const char* op = "==";
                switch (expr.binOp) {
                    case BinOp::Eq: op = "=="; break;
                    case BinOp::Ne: op = "!="; break;
                    case BinOp::Lt: op = "<"; break;
                    case BinOp::Le: op = "<="; break;
                    case BinOp::Gt: op = ">"; break;
                    case BinOp::Ge: op = ">="; break;
                    default: break;
                }
                return "((" + lhs + " " + op + " " + rhs + ") ? static_cast<std::int8_t>(-1) : "
                       "static_cast<std::int8_t>(0))";
            }

            const char* op = "+";
            switch (expr.binOp) {
                case BinOp::Add: op = "+"; break;
                case BinOp::Sub: op = "-"; break;
                case BinOp::Mul: op = "*"; break;
                case BinOp::Div: op = "/"; break;
                case BinOp::IDiv: op = "/"; break;
                case BinOp::Mod: op = "%"; break;
                case BinOp::Shl: op = "<<"; break;
                case BinOp::Shr: op = ">>"; break;
                case BinOp::Concat: op = "+"; break;
                case BinOp::And: op = "&"; break;
                case BinOp::Or: op = "|"; break;
                case BinOp::Xor: op = "^"; break;
                case BinOp::Pow: op = "^"; break; // only reached for a UserDefined operand's own overload
                default: break;
            }
            return "(" + lhs + " " + op + " " + rhs + ")";
        }
    }
    throw std::runtime_error("codegen: unknown expression kind");
}

std::string Codegen::ind(int indent) { return std::string(static_cast<size_t>(indent) * 4, ' '); }

std::string Codegen::nextName(const std::string& prefix) {
    return prefix + std::to_string(tempCounter_++);
}

std::string Codegen::genCondition(const Expr& expr) {
    return "(" + genExpr(expr) + " != 0)";
}

void Codegen::genBlock(const std::vector<StmtPtr>& stmts, std::ostringstream& out, int indent) {
    for (const auto& stmt : stmts) {
        genStmt(*stmt, out, indent);
    }
}

void Codegen::genStmt(const Stmt& stmt, std::ostringstream& out, int indent) {
    switch (stmt.kind) {
        case StmtKind::Dim: {
            if (!stmt.isArray) {
                out << ind(indent) << cppType(stmt.declaredType) << " " << mangleName(stmt.name)
                    << "{};\n";
                return;
            }
            std::string lowerVar = mangleName(stmt.name) + "__lo";
            if (!stmt.arrayUpper) {
                /// DIM arr() AS Type: a dynamic array, empty until REDIM'd.
                out << ind(indent) << "auto " << lowerVar << " = 0;\n";
                out << ind(indent) << "std::vector<" << cppType(stmt.declaredType) << "> "
                    << mangleName(stmt.name) << ";\n";
            } else {
                out << ind(indent) << "auto " << lowerVar << " = "
                    << (stmt.arrayLower ? genExpr(*stmt.arrayLower) : "0") << ";\n";
                out << ind(indent) << "std::vector<" << cppType(stmt.declaredType) << "> "
                    << mangleName(stmt.name) << "(static_cast<std::size_t>((" << genExpr(*stmt.arrayUpper)
                    << ") - " << lowerVar << " + 1));\n";
            }
            arrayLowerBoundVar_[canonicalName(stmt.name)] = lowerVar;
            return;
        }
        case StmtKind::Redim: {
            const std::string& lowerVar = arrayLowerBoundVar_.at(canonicalName(stmt.name));
            out << ind(indent) << lowerVar << " = "
                << (stmt.arrayLower ? genExpr(*stmt.arrayLower) : "0") << ";\n";
            std::string sizeExpr = "static_cast<std::size_t>((" + genExpr(*stmt.arrayUpper) + ") - " +
                                    lowerVar + " + 1)";
            if (stmt.preserve) {
                out << ind(indent) << mangleName(stmt.name) << ".resize(" << sizeExpr << ");\n";
            } else {
                out << ind(indent) << mangleName(stmt.name) << ".assign(" << sizeExpr << ", {});\n";
            }
            return;
        }
        case StmtKind::Const:
            out << ind(indent) << "const " << cppType(stmt.declaredType) << " " << mangleName(stmt.name)
                << " = " << genExpr(*stmt.expr) << ";\n";
            return;
        case StmtKind::Enum: {
            for (const EnumMember& member : stmt.enumMembers) {
                out << ind(indent) << "constexpr std::int32_t " << mangleName(member.name) << " = "
                    << member.resolvedValue << ";\n";
            }
            return;
        }
        case StmtKind::Assign: {
            if (stmt.isReturnAssign) {
                out << ind(indent) << "eb__ret = " << genExpr(*stmt.expr) << ";\n";
                return;
            }
            if (stmt.target) {
                /// A PROPERTY setter needs a completely different shape
                /// (`.eb_name_set(value)`, a method call) than a plain
                /// assignment - can't just call genExpr on the whole target,
                /// since that produces the *getter* form instead.
                if (stmt.target->kind == ExprKind::Member && stmt.target->isProperty) {
                    out << ind(indent) << memberReceiverPrefix(*stmt.target->lhs)
                        << mangleName(stmt.target->stringValue) << "_set(" << genExpr(*stmt.expr)
                        << ");\n";
                    return;
                }
                /// A general Member/Call-chain lvalue (obj.field, arr(i).field,
                /// ...) - genExpr already produces a valid C++ lvalue for it.
                out << ind(indent) << genExpr(*stmt.target) << " = " << genExpr(*stmt.expr) << ";\n";
                return;
            }
            if (stmt.index) {
                const std::string& lowerVar = arrayLowerBoundVar_.at(canonicalName(stmt.name));
                out << ind(indent) << mangleName(stmt.name) << "[static_cast<std::size_t>(("
                    << genExpr(*stmt.index) << ") - " << lowerVar << ")] = " << genExpr(*stmt.expr)
                    << ";\n";
                return;
            }
            out << ind(indent) << mangleName(stmt.name) << " = " << genExpr(*stmt.expr) << ";\n";
            return;
        }
        case StmtKind::Print: {
            out << ind(indent) << "::ebasic::rt::printLine(";
            for (size_t i = 0; i < stmt.args.size(); ++i) {
                if (i > 0) out << ", ";
                out << genExpr(*stmt.args[i]);
            }
            out << ");\n";
            return;
        }
        case StmtKind::If: {
            out << ind(indent) << "if (" << genCondition(*stmt.conditions[0]) << ") {\n";
            genBlock(stmt.blocks[0], out, indent + 1);
            out << ind(indent) << "}\n";
            for (size_t i = 1; i < stmt.conditions.size(); ++i) {
                out << ind(indent) << "else if (" << genCondition(*stmt.conditions[i]) << ") {\n";
                genBlock(stmt.blocks[i], out, indent + 1);
                out << ind(indent) << "}\n";
            }
            if (stmt.hasElse) {
                out << ind(indent) << "else {\n";
                genBlock(stmt.blocks.back(), out, indent + 1);
                out << ind(indent) << "}\n";
            }
            return;
        }
        case StmtKind::SelectCase: {
            std::string sel = nextName("eb__sel");
            out << ind(indent) << "{\n";
            out << ind(indent + 1) << "auto " << sel << " = " << genExpr(*stmt.expr) << ";\n";
            bool first = true;
            for (const CaseArm& arm : stmt.cases) {
                if (arm.isElse) {
                    out << ind(indent + 1) << (first ? "{\n" : "else {\n");
                    genBlock(arm.body, out, indent + 2);
                    out << ind(indent + 1) << "}\n";
                    continue;
                }
                out << ind(indent + 1) << (first ? "if (" : "else if (");
                first = false;
                for (size_t j = 0; j < arm.matches.size(); ++j) {
                    if (j > 0) out << " || ";
                    out << sel << " == " << genExpr(*arm.matches[j]);
                }
                out << ") {\n";
                genBlock(arm.body, out, indent + 2);
                out << ind(indent + 1) << "}\n";
            }
            out << ind(indent) << "}\n";
            return;
        }
        case StmtKind::ForNext: {
            std::string endTmp = nextName("eb__forend");
            std::string stepTmp = nextName("eb__forstep");
            std::string label = nextName("eb__forexit");
            std::string var = mangleName(stmt.name);

            loopStack_.emplace_back(LoopKind::For, label);
            out << ind(indent) << "{\n";
            out << ind(indent + 1) << "auto " << endTmp << " = " << genExpr(*stmt.forEnd) << ";\n";
            out << ind(indent + 1) << "auto " << stepTmp << " = "
                << (stmt.forStep ? genExpr(*stmt.forStep) : "1") << ";\n";
            out << ind(indent + 1) << var << " = " << genExpr(*stmt.expr) << ";\n";
            out << ind(indent + 1) << "if (" << stepTmp << " >= 0) {\n";
            out << ind(indent + 2) << "for (; " << var << " <= " << endTmp << "; " << var
                << " += " << stepTmp << ") {\n";
            genBlock(stmt.body, out, indent + 3);
            out << ind(indent + 2) << "}\n";
            out << ind(indent + 1) << "} else {\n";
            out << ind(indent + 2) << "for (; " << var << " >= " << endTmp << "; " << var
                << " += " << stepTmp << ") {\n";
            genBlock(stmt.body, out, indent + 3);
            out << ind(indent + 2) << "}\n";
            out << ind(indent + 1) << "}\n";
            out << ind(indent) << "}\n";
            out << label << ": ;\n";
            loopStack_.pop_back();
            return;
        }
        case StmtKind::DoLoop: {
            std::string label = nextName("eb__doexit");
            loopStack_.emplace_back(LoopKind::Do, label);
            out << ind(indent) << "{\n";
            out << ind(indent + 1) << "for (;;) {\n";
            if (stmt.preTest == LoopTest::While) {
                out << ind(indent + 2) << "if (!" << genCondition(*stmt.preCond) << ") break;\n";
            } else if (stmt.preTest == LoopTest::Until) {
                out << ind(indent + 2) << "if (" << genCondition(*stmt.preCond) << ") break;\n";
            }
            genBlock(stmt.body, out, indent + 2);
            if (stmt.postTest == LoopTest::While) {
                out << ind(indent + 2) << "if (!" << genCondition(*stmt.postCond) << ") break;\n";
            } else if (stmt.postTest == LoopTest::Until) {
                out << ind(indent + 2) << "if (" << genCondition(*stmt.postCond) << ") break;\n";
            }
            out << ind(indent + 1) << "}\n";
            out << ind(indent) << "}\n";
            out << label << ": ;\n";
            loopStack_.pop_back();
            return;
        }
        case StmtKind::WhileWend: {
            std::string label = nextName("eb__whileexit");
            loopStack_.emplace_back(LoopKind::While, label);
            out << ind(indent) << "{\n";
            out << ind(indent + 1) << "while (" << genCondition(*stmt.expr) << ") {\n";
            genBlock(stmt.body, out, indent + 2);
            out << ind(indent + 1) << "}\n";
            out << ind(indent) << "}\n";
            out << label << ": ;\n";
            loopStack_.pop_back();
            return;
        }
        case StmtKind::Goto:
            out << ind(indent) << "goto " << mangleName(stmt.name) << ";\n";
            return;
        case StmtKind::Label:
            out << mangleName(stmt.name) << ": ;\n";
            return;
        case StmtKind::ExitLoop: {
            /// EXIT SUB/FUNCTION need no label/goto: a plain C++ `return`
            /// already unwinds out of any number of enclosing loops within
            /// the current function, which is exactly what they mean.
            if (stmt.exitKind == LoopKind::Sub) {
                out << ind(indent) << "return;\n";
                return;
            }
            if (stmt.exitKind == LoopKind::Function) {
                out << ind(indent) << "return eb__ret;\n";
                return;
            }
            for (auto it = loopStack_.rbegin(); it != loopStack_.rend(); ++it) {
                if (it->first == stmt.exitKind) {
                    out << ind(indent) << "goto " << it->second << ";\n";
                    return;
                }
            }
            throw std::runtime_error("codegen: EXIT with no matching enclosing loop (sema should "
                                      "have caught this)");
        }
        case StmtKind::SubDecl:
        case StmtKind::FunctionDecl:
            throw std::runtime_error("codegen: SUB/FUNCTION must be top-level (sema should have "
                                      "caught this)");
        case StmtKind::TypeDecl:
        case StmtKind::UnionDecl:
            throw std::runtime_error("codegen: TYPE/UNION must be top-level (sema should have caught "
                                      "this)");
        case StmtKind::NamespaceDecl:
            throw std::runtime_error("codegen: NAMESPACE must be top-level (sema should have caught "
                                      "this)");
        case StmtKind::CallStmt: {
            /// Same shapes as the expression-position Call: a
            /// Namespace.Name(args) qualifier (`::`), a This.Method(args)
            /// receiver (`->`), a Base.Method(args) qualified non-virtual
            /// call (`eb_base::`), an obj.Method(args) receiver (`.`), or a
            /// plain free-function call (no prefix).
            std::string calleeName = resolveCalleeName(stmt.target.get(), stmt.name);
            out << ind(indent) << calleeName << "(";
            for (size_t i = 0; i < stmt.args.size(); ++i) {
                if (i > 0) out << ", ";
                out << genExpr(*stmt.args[i]);
            }
            out << ");\n";
            return;
        }
        case StmtKind::Return:
            if (stmt.expr) {
                out << ind(indent) << "return " << genExpr(*stmt.expr) << ";\n";
            } else {
                out << ind(indent) << "return;\n";
            }
            return;
        case StmtKind::GoSub:
            out << ind(indent) << gosubFunctionName(stmt.name) << "();\n";
            return;
    }
}

void Codegen::genTypeDecl(const Stmt& stmt) {
    std::string key = canonicalName(stmt.name);
    if (typesEmitted_.count(key)) return;
    if (!typesBeingEmitted_.insert(key).second) {
        /// Circular embedding (TypeA contains a TypeB field, TypeB contains a
        /// TypeA field, ...) - impossible as a real, finite-size C++
        /// aggregate. Break the recursion here rather than looping forever;
        /// the backend will reject whichever struct ends up incomplete.
        return;
    }

    /// M4d: a TYPE with zero fields, zero methods (which also covers
    /// properties/ctor/dtor - all stored in `stmt.methods`), and no EXTENDS is
    /// an opaque external handle (Sema's RecordInfo::isOpaque, recomputed
    /// here directly from the AST since Codegen has no access to Sema's
    /// internal tables). Emit a bare forward declaration only - never a `{
    /// };` body, which would wrongly claim a concrete, empty-but-complete
    /// layout against the real library's actual, unknown one. Only ever legal
    /// via PTR; Sema already rejects by-value DIM/embedding/EXTENDS of it.
    if (stmt.kind == StmtKind::TypeDecl && stmt.fields.empty() && stmt.methods.empty() &&
        stmt.baseTypeName.empty()) {
        typesOut_ << "struct " << mangleName(stmt.name) << ";\n\n";
        typesBeingEmitted_.erase(key);
        typesEmitted_.insert(key);
        return;
    }

    for (const FieldDecl& field : stmt.fields) {
        if (field.type.kind != TypeKind::UserDefined) continue;
        auto it = typeDeclsByName_.find(canonicalName(field.type.typeName));
        if (it != typeDeclsByName_.end()) genTypeDecl(*it->second);
    }
    /// A base class needs its *full* definition above the derived struct too
    /// (inheriting from an incomplete type is illegal, same reason an
    /// embedded-by-value field does) - so it's a dependency exactly like one.
    if (!stmt.baseTypeName.empty()) {
        auto baseIt = typeDeclsByName_.find(canonicalName(stmt.baseTypeName));
        if (baseIt != typeDeclsByName_.end()) genTypeDecl(*baseIt->second);
    }

    /// A C++ union may have at most one member with a default (in-class)
    /// initializer - so unlike a struct's per-field `{}`, a union's members
    /// are declared bare and the whole object is zero-initialized instead
    /// wherever it's DIM'd (`Type var{};`, already emitted unconditionally
    /// by the Dim codegen below), matching FreeBASIC's own "otherwise
    /// initializes to zero" default (STRING*N fields, the one exception,
    /// are Sema-rejected from UNIONs entirely - see collectTypes).
    bool isUnion = stmt.kind == StmtKind::UnionDecl;
    typesOut_ << (isUnion ? "union " : "struct ") << mangleName(stmt.name);
    if (!stmt.baseTypeName.empty()) {
        typesOut_ << " : public " << mangleName(stmt.baseTypeName);
    }
    typesOut_ << " {\n";
    for (const FieldDecl& field : stmt.fields) {
        typesOut_ << ind(1) << cppType(field.type) << " " << mangleName(field.name);
        if (!isUnion) typesOut_ << "{}";
        typesOut_ << ";\n";
    }
    /// Method/constructor/destructor *declarations* only (real C++ member
    /// declarations, matching FreeBASIC's own "declared within" half) - the
    /// body lives in a separate out-of-line definition, emitted by
    /// genMethodDefinition. UNION never reaches here with any (Sema-rejected).
    for (const StmtPtr& method : stmt.methods) {
        if (method->isCtor) {
            typesOut_ << ind(1) << mangleName(stmt.name) << "();\n";
            continue;
        }
        if (method->isDtor) {
            typesOut_ << ind(1) << "~" << mangleName(stmt.name) << "();\n";
            continue;
        }
        if (method->isProperty) {
            /// A PROPERTY has no native C++ syntax - declared as two plain
            /// methods, `_get`/`_set`, rewritten from field-like access at
            /// every use site by genExpr(Member)/Assign's own codegen.
            bool isGetter = method->kind == StmtKind::FunctionDecl;
            std::string retType = isGetter ? cppType(method->declaredType) : "void";
            typesOut_ << ind(1) << retType << " " << mangleName(method->name)
                       << (isGetter ? "_get(" : "_set(")
                       << buildParamList(method->params, /*includeDefaults=*/true) << ");\n";
            continue;
        }
        bool isFunction = method->kind == StmtKind::FunctionDecl;
        std::string retType = isFunction ? cppType(method->declaredType) : "void";
        /// `Override` implies participation in the vtable too, whether or
        /// not `Virtual` was also explicitly written (Sema's own rule).
        bool isVirtual = method->isVirtual || method->isOverride;
        typesOut_ << ind(1) << (isVirtual ? "virtual " : "") << retType << " "
                   << mangleName(method->name) << "("
                   << buildParamList(method->params, /*includeDefaults=*/true) << ")"
                   << (method->isOverride ? " override" : "") << ";\n";
    }
    typesOut_ << "};\n\n";

    typesBeingEmitted_.erase(key);
    typesEmitted_.insert(key);
}

std::string Codegen::buildParamList(const std::vector<Param>& params, bool includeDefaults) {
    std::string paramList;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) paramList += ", ";
        const Param& p = params[i];
        paramList += cppType(p.type);
        paramList += p.byRef ? "& " : " ";
        paramList += mangleName(p.name);
        /// A real C++ default argument - only ever legal on the *first*
        /// signature the translation unit sees (repeating it identically on
        /// a later re-declaration, e.g. an out-of-line body following its
        /// own in-class/forward-declared prototype, is a hard "redefinition
        /// of default argument" error), so callers pass `includeDefaults =
        /// false` for that second emission - see genProcedure/
        /// genMethodDefinition.
        if (includeDefaults && p.defaultValue) {
            paramList += " = " + genExpr(*p.defaultValue);
        }
    }
    return paramList;
}

std::string Codegen::buildOperatorParamList(const std::vector<Param>& params) {
    std::string paramList;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) paramList += ", ";
        const Param& p = params[i];
        if (p.type.kind == TypeKind::UserDefined) {
            paramList += "const " + cppType(p.type) + "& ";
        } else {
            paramList += cppType(p.type) + " ";
        }
        paramList += mangleName(p.name);
    }
    return paramList;
}

std::string Codegen::cppOperatorToken(BinOp op) {
    switch (op) {
        case BinOp::Add: return "+";
        case BinOp::Sub: return "-";
        case BinOp::Mul: return "*";
        case BinOp::Div: return "/";
        case BinOp::IDiv: return "/";
        case BinOp::Mod: return "%";
        case BinOp::Pow: return "^";
        case BinOp::Shl: return "<<";
        case BinOp::Shr: return ">>";
        case BinOp::Concat: return "+";
        case BinOp::Eq: return "==";
        case BinOp::Ne: return "!=";
        case BinOp::Lt: return "<";
        case BinOp::Le: return "<=";
        case BinOp::Gt: return ">";
        case BinOp::Ge: return ">=";
        case BinOp::And: return "&";
        case BinOp::Or: return "|";
        case BinOp::Xor: return "^";
    }
    throw std::runtime_error("codegen: unknown BinOp for operator overload");
}

void Codegen::genProcedure(const Stmt& stmt) {
    bool isFunction = stmt.kind == StmtKind::FunctionDecl;
    std::string retType = isFunction ? cppType(stmt.declaredType) : "void";

    if (stmt.isExtern) {
        /// A DECLARE/EXTERN signature (M4): no eBasic-side body at all - the
        /// real definition lives in an external C/C++ library, so only a
        /// prototype is ever emitted, using the real external name
        /// *verbatim* (never mangleName, which would rename it to something
        /// the linker can't find). "C" linkage needs `extern "C"` so the
        /// real symbol isn't C++-mangled; "C++" linkage needs no wrapping at
        /// all - it's already a normal, real C++ declaration, the concrete
        /// payoff of transpiling to real C++ rather than emulating mangling.
        std::string externName = stmt.externAlias.empty() ? stmt.name : stmt.externAlias;
        std::string paramList = buildParamList(stmt.params, /*includeDefaults=*/true);
        bool wrapC = stmt.externLinkage == "C";
        if (wrapC) protoOut_ << "extern \"C\" {\n";
        protoOut_ << retType << " " << externName << "(" << paramList << ");\n";
        if (wrapC) protoOut_ << "}\n";
        return;
    }

    std::string name = stmt.isOperator ? ("operator" + cppOperatorToken(stmt.operatorBinOp))
                                        : mangleName(stmt.name);
    /// Shared-library support: an isExported procedure (a real, bodied
    /// `Sub`/`Function` written inside `Extern "C" ... End Extern`) is the
    /// *only* definition Codegen ever emits for it - reusing the ordinary
    /// body-emission path below, just under its own verbatim externAlias
    /// name (never mangleName, since dlopen/dlsym or a plain external C
    /// caller must find it exactly as written) and wrapped in real dynamic-
    /// export linkage. EBASIC_EXPORT (defined in generate()'s preamble only
    /// when sharedLib_ is true) adds real dllexport/visibility-default;
    /// otherwise plain `extern "C"` still gives it unmangled C linkage
    /// without claiming dynamic-export visibility that wouldn't mean
    /// anything here (e.g. an ordinary `--lib`/executable build that merely
    /// reuses the export syntax - no real shared object exists to export
    /// from).
    std::string exportPrefix;
    if (stmt.isExported) {
        name = stmt.externAlias.empty() ? stmt.name : stmt.externAlias;
        exportPrefix = sharedLib_ ? "EBASIC_EXPORT " : "extern \"C\" ";
    }
    /// Two separate renderings, not one reused string: a default argument
    /// may only appear on the *first* signature emission the translation
    /// unit sees (see buildParamList's own doc comment) - the prototype
    /// gets it, the body definition doesn't.
    std::string protoParamList =
        stmt.isOperator ? buildOperatorParamList(stmt.params)
                         : buildParamList(stmt.params, /*includeDefaults=*/true);
    std::string defParamList =
        stmt.isOperator ? protoParamList
                         : buildParamList(stmt.params, /*includeDefaults=*/false);

    protoOut_ << exportPrefix << retType << " " << name << "(" << protoParamList << ");\n";

    procOut_ << exportPrefix << retType << " " << name << "(" << defParamList << ") {\n";
    if (isFunction) {
        procOut_ << ind(1) << retType << " eb__ret{};\n";
    }
    /// A local array can shadow a global of the same name; save/restore so
    /// that mapping doesn't leak into code generated after this procedure.
    auto savedArrayLowerBounds = arrayLowerBoundVar_;
    genBlock(stmt.body, procOut_, 1);
    arrayLowerBoundVar_ = savedArrayLowerBounds;
    if (isFunction) {
        procOut_ << ind(1) << "return eb__ret;\n";
    }
    procOut_ << "}\n\n";
}

void Codegen::genMethodDefinition(const Stmt& stmt) {
    bool isFunction = stmt.kind == StmtKind::FunctionDecl;
    std::string owner = mangleName(stmt.ownerType);
    /// The in-class declaration (genTypeDecl) already rendered any default
    /// argument - this out-of-line body definition must not repeat it (a
    /// hard C++ error), so `includeDefaults = false`.
    std::string paramList = buildParamList(stmt.params, /*includeDefaults=*/false);

    /// A constructor/destructor has no return type at all in C++ (not even
    /// `void`) and is named after the owning TYPE itself; a real method
    /// gets `Owner::eb_name`, same as any other out-of-line C++ member
    /// definition.
    std::string retType;
    std::string qualifiedName;
    if (stmt.isCtor) {
        qualifiedName = owner + "::" + owner;
    } else if (stmt.isDtor) {
        qualifiedName = owner + "::~" + owner;
    } else {
        retType = (isFunction ? cppType(stmt.declaredType) : "void") + " ";
        /// A PROPERTY's getter/setter share one FreeBASIC identifier but
        /// need two distinct C++ method names (matching genTypeDecl's own
        /// declarations) - `isFunction` (has a return type) already tells
        /// getter from setter, same as for the declaration.
        qualifiedName = owner + "::" + mangleName(stmt.name) + (stmt.isProperty ? (isFunction ? "_get" : "_set") : "");
    }

    procOut_ << retType << qualifiedName << "(" << paramList << ") {\n";
    if (isFunction) {
        procOut_ << ind(1) << cppType(stmt.declaredType) << " eb__ret{};\n";
    }
    /// A local array can shadow a global of the same name; save/restore so
    /// that mapping doesn't leak into code generated after this method.
    auto savedArrayLowerBounds = arrayLowerBoundVar_;
    std::string savedOwnerType = currentOwnerType_;
    currentOwnerType_ = canonicalName(stmt.ownerType);
    genBlock(stmt.body, procOut_, 1);
    currentOwnerType_ = savedOwnerType;
    arrayLowerBoundVar_ = savedArrayLowerBounds;
    if (isFunction) {
        procOut_ << ind(1) << "return eb__ret;\n";
    }
    procOut_ << "}\n\n";
}

void Codegen::genNamespaceDecl(const Stmt& stmt) {
    std::string ns = mangleName(stmt.name);
    /// TYPE can't be nested inside a NAMESPACE (Sema-enforced), so typesOut_
    /// never gets content here - skip wrapping it at all. Only wrap
    /// proto/proc/globals if this namespace actually contributes to them, to
    /// avoid emitting empty `namespace eb_x { }` noise.
    bool hasProcs = false;
    bool hasGlobals = false;
    /// A "purely extern" NAMESPACE (M4c: every member is an EXTERN/DECLARE
    /// binding, e.g. `Extern "C++" Namespace ebfixture ... End Namespace`)
    /// must NOT be wrapped in this BASIC-side eb_-mangled namespace at all:
    /// each member's externAlias already carries its own fully-qualified
    /// real external name (e.g. "ebfixture::Square"), computed once at
    /// parse time - nesting that inside `namespace eb_ebfixture { ... }`
    /// would look for the wrong, doubly-qualified symbol.
    bool isPureExtern = true;
    for (const auto& memberPtr : stmt.body) {
        if (memberPtr->kind == StmtKind::SubDecl || memberPtr->kind == StmtKind::FunctionDecl) {
            hasProcs = true;
            if (!memberPtr->isExtern) isPureExtern = false;
        } else if (memberPtr->kind == StmtKind::Dim || memberPtr->kind == StmtKind::Const ||
                   memberPtr->kind == StmtKind::Enum) {
            hasGlobals = true;
            isPureExtern = false;
        }
    }
    if (hasProcs && isPureExtern) {
        /// A qualified standalone declaration (`RetType realNs::Member(...)`)
        /// requires `realNs` to already be an open namespace somewhere in
        /// the translation unit - it isn't, so genProcedure's own
        /// (unqualified) name is instead wrapped in a real
        /// `namespace realNs { ... }` block here, using the namespace's
        /// real (possibly aliased) external name, never mangleName. Every
        /// isExtern member has no body at all, so only protoOut_ ever gets
        /// anything - no need to also open this block in procOut_.
        std::string realNs = stmt.externAlias.empty() ? stmt.name : stmt.externAlias;
        protoOut_ << "namespace " << realNs << " {\n";
        for (const auto& memberPtr : stmt.body) {
            genProcedure(*memberPtr);
        }
        protoOut_ << "} // namespace " << realNs << "\n";
        return;
    }

    if (hasProcs) {
        protoOut_ << "namespace " << ns << " {\n";
        procOut_ << "namespace " << ns << " {\n";
    }
    if (hasGlobals) globalsOut_ << "namespace " << ns << " {\n";

    for (const auto& memberPtr : stmt.body) {
        const Stmt& member = *memberPtr;
        if (member.kind == StmtKind::SubDecl || member.kind == StmtKind::FunctionDecl) {
            genProcedure(member);
        } else if (member.kind == StmtKind::Dim || member.kind == StmtKind::Const ||
                   member.kind == StmtKind::Enum) {
            genStmt(member, globalsOut_, 1);
        }
        /// Sema already rejects anything else (incl. nested TYPE/NAMESPACE)
        /// directly inside a NAMESPACE.
    }

    if (hasProcs) {
        protoOut_ << "} // namespace " << ns << "\n";
        procOut_ << "} // namespace " << ns << "\n";
    }
    if (hasGlobals) globalsOut_ << "} // namespace " << ns << "\n";
}

std::string Codegen::generate(const Module& module, bool libMode, bool sharedLib) {
    externLibs_ = module.externLibs;
    sharedLib_ = sharedLib;

    /// Top-level DIM/CONST/ENUM become real C++ globals (declared before any
    /// function bodies) so SUB/FUNCTION can see them; SUB/FUNCTION become
    /// separate C++ functions (prototype + body); everything else still runs
    /// in main(), in its original relative order. Sema already enforces
    /// declare-before-use sequentially (including for globals referenced from
    /// a procedure body), so bucketing all globals before all procedures is
    /// always a safe superset of that ordering.
    ///
    /// A GOSUB-target Label starts a span of statements hoisted into their
    /// own synthesized function (ending at the next Label, of any kind, or
    /// end of program) instead of running inline in main() - see
    /// gosubFunctionName's doc comment. Declarative statements (DIM/CONST/
    /// ENUM/SUB/FUNCTION) are never part of that span; they're bucketed the
    /// same regardless of whether a GOSUB span is currently open.
    for (const auto& stmtPtr : module.stmts) {
        if (stmtPtr->kind == StmtKind::GoSub) gosubTargets_.insert(canonicalName(stmtPtr->name));
        else if (stmtPtr->kind == StmtKind::TypeDecl || stmtPtr->kind == StmtKind::UnionDecl) {
            typeDeclsByName_[canonicalName(stmtPtr->name)] = stmtPtr.get();
            if (!stmtPtr->baseTypeName.empty()) {
                baseTypeOf_[canonicalName(stmtPtr->name)] = canonicalName(stmtPtr->baseTypeName);
            }
        } else if (stmtPtr->kind == StmtKind::NamespaceDecl) {
            namespaces_.insert(canonicalName(stmtPtr->name));
        }
    }
    collectExternProcNames(module.stmts, "", "");

    /// Forward-declare every TYPE/UNION before any full definition. A pointer
    /// field (self-referential, e.g. a linked-list Node, or pointing at
    /// another TYPE/UNION declared later in the file) only needs the
    /// pointee's name in scope, not its full definition - genTypeDecl's
    /// dependency ordering below only pulls in embedded-by-value fields, so a
    /// bare pointer target might otherwise reach the backend as an
    /// undeclared type. The forward declaration's class-key (`struct` vs
    /// `union`) must match the eventual definition's - a mismatch is
    /// ill-formed C++, unlike C's more permissive elaborated-type-specifier
    /// rules.
    for (const auto& [name, declStmt] : typeDeclsByName_) {
        bool isUnion = declStmt->kind == StmtKind::UnionDecl;
        typesOut_ << (isUnion ? "union " : "struct ") << mangleName(name) << ";\n";
    }
    if (!typeDeclsByName_.empty()) typesOut_ << "\n";

    std::ostringstream mainOut;
    bool insideGosub = false;

    auto closeGosubIfOpen = [&]() {
        if (insideGosub) {
            procOut_ << ind(1) << "return;\n}\n\n";
            insideGosub = false;
        }
    };

    for (const auto& stmtPtr : module.stmts) {
        const Stmt& stmt = *stmtPtr;
        if (stmt.kind == StmtKind::Label) {
            closeGosubIfOpen();
            if (gosubTargets_.count(canonicalName(stmt.name))) {
                std::string fn = gosubFunctionName(stmt.name);
                protoOut_ << "void " << fn << "();\n";
                procOut_ << "void " << fn << "() {\n";
                insideGosub = true;
            } else {
                genStmt(stmt, mainOut, 1);
            }
        } else if (stmt.kind == StmtKind::SubDecl || stmt.kind == StmtKind::FunctionDecl) {
            if (stmt.ownerType.empty()) {
                genProcedure(stmt);
            } else {
                genMethodDefinition(stmt);
            }
        } else if (stmt.kind == StmtKind::TypeDecl || stmt.kind == StmtKind::UnionDecl) {
            genTypeDecl(stmt);
        } else if (stmt.kind == StmtKind::NamespaceDecl) {
            genNamespaceDecl(stmt);
        } else if (stmt.kind == StmtKind::Dim || stmt.kind == StmtKind::Const ||
                   stmt.kind == StmtKind::Enum) {
            genStmt(stmt, globalsOut_, 0);
        } else if (insideGosub) {
            genStmt(stmt, procOut_, 1);
        } else {
            genStmt(stmt, mainOut, 1);
        }
    }
    closeGosubIfOpen();

    std::ostringstream out;
    out << "// Generated by ebc. Do not edit.\n";
    out << "#include \"ebasic/runtime/runtime.hpp\"\n";
    out << "#include <cmath>\n";
    out << "#include <cstdint>\n";
    out << "#include <vector>\n\n";
    /// Shared-library support: only meaningful (and only emitted) when this
    /// is a real `--shared-lib`/`-dll` build - an isExported procedure's
    /// definition uses this macro instead of plain `extern "C"` (see
    /// genProcedure) to also get real dynamic-export visibility.
    if (sharedLib_) {
        out << "#if defined(_WIN32)\n";
        out << "#define EBASIC_EXPORT extern \"C\" __declspec(dllexport)\n";
        out << "#else\n";
        out << "#define EBASIC_EXPORT extern \"C\" __attribute__((visibility(\"default\")))\n";
        out << "#endif\n\n";
    }
    if (!typesOut_.str().empty()) out << typesOut_.str();
    if (!globalsOut_.str().empty()) out << globalsOut_.str() << "\n";
    if (!protoOut_.str().empty()) out << protoOut_.str() << "\n";
    out << procOut_.str();
    /// M5 library build: no main() at all - the driver has already verified
    /// there are no top-level executable statements to put in one (mainOut
    /// is guaranteed empty here), and a library's object file must never
    /// define `main` itself (it would collide with the consuming package's
    /// own `main` at final link time).
    if (!libMode) {
        out << "int main(int argc, char** argv) {\n";
        out << "    ::ebasic::rt::processlib::setCommandLineArgs(argc, argv);\n";
        out << mainOut.str();
        out << "    return 0;\n";
        out << "}\n";
    }
    return out.str();
}

std::string Codegen::basicTypeName(const Type& type) {
    switch (type.kind) {
        case TypeKind::Byte: return "BYTE";
        case TypeKind::UByte: return "UBYTE";
        case TypeKind::Short: return "SHORT";
        case TypeKind::UShort: return "USHORT";
        case TypeKind::Integer: return "INTEGER";
        case TypeKind::Long: return "LONG";
        case TypeKind::UInteger: return "UINTEGER";
        case TypeKind::LongInt: return "LONGINT";
        case TypeKind::ULongInt: return "ULONGINT";
        case TypeKind::Single: return "SINGLE";
        case TypeKind::Double: return "DOUBLE";
        case TypeKind::Boolean: return "BOOLEAN";
        case TypeKind::StringT: return "STRING";
        case TypeKind::ZStringT: return "ZSTRING";
        case TypeKind::UserDefined: return type.typeName;
        case TypeKind::Pointer:
            return (type.pointee ? basicTypeName(*type.pointee) : std::string("ANY")) + " PTR";
        case TypeKind::Unknown: return "INTEGER"; // unreachable for a resolved signature
    }
    return "INTEGER";
}

std::string Codegen::generateLibraryInterface(const Module& module, const std::string& libName) {
    std::ostringstream typesText;
    std::ostringstream declsText;
    std::ostringstream skippedText;

    /// Canonical TYPE/UNION name -> its own Stmt, so a derived record's
    /// EXTENDS chain can be walked without a second pass over module.stmts.
    std::unordered_map<std::string, const Stmt*> typesByName;
    for (const auto& stmtPtr : module.stmts) {
        if (stmtPtr->kind == StmtKind::TypeDecl || stmtPtr->kind == StmtKind::UnionDecl) {
            typesByName[canonicalName(stmtPtr->name)] = stmtPtr.get();
        }
    }

    /// A plain-data or opaque record (no methods/ctor/dtor anywhere in its
    /// own EXTENDS chain, transitively) is safe to duplicate verbatim
    /// across translation units, exactly like a C header's struct
    /// definition is - a record with methods is not (yet) exportable
    /// across a library boundary (no equivalent mechanism exists yet for
    /// calling a method across it), and neither is a record that EXTENDS
    /// one, even if it adds no methods of its own: the generated `EXTENDS
    /// Base` line would otherwise reference a TYPE this interface never
    /// declares.
    std::function<bool(const Stmt&)> isExportablePlainData = [&](const Stmt& s) -> bool {
        if (!s.methods.empty()) return false;
        if (s.baseTypeName.empty()) return true;
        auto it = typesByName.find(canonicalName(s.baseTypeName));
        return it != typesByName.end() && isExportablePlainData(*it->second);
    };

    /// Emits `s` (and, first, its base - EXTENDS's dependency, exactly like
    /// genTypeDecl's own field/base handling) at most once, regardless of
    /// how many derived types or plain field references reach it first.
    std::unordered_set<std::string> typesEmitted;
    std::function<void(const Stmt&)> emitType = [&](const Stmt& s) {
        std::string key = canonicalName(s.name);
        if (!typesEmitted.insert(key).second) return;
        if (!s.baseTypeName.empty()) {
            auto it = typesByName.find(canonicalName(s.baseTypeName));
            if (it != typesByName.end()) emitType(*it->second);
        }
        typesText << (s.kind == StmtKind::UnionDecl ? "UNION " : "TYPE ") << s.name;
        if (!s.baseTypeName.empty()) typesText << " EXTENDS " << s.baseTypeName;
        typesText << "\n";
        for (const FieldDecl& field : s.fields) {
            typesText << "    " << field.name << " AS " << basicTypeName(field.type) << "\n";
        }
        typesText << (s.kind == StmtKind::UnionDecl ? "END UNION" : "END TYPE") << "\n\n";
    };

    for (const auto& stmtPtr : module.stmts) {
        const Stmt& stmt = *stmtPtr;
        if ((stmt.kind == StmtKind::TypeDecl || stmt.kind == StmtKind::UnionDecl) &&
            isExportablePlainData(stmt)) {
            emitType(stmt);
        } else if ((stmt.kind == StmtKind::SubDecl || stmt.kind == StmtKind::FunctionDecl) &&
                   stmt.ownerType.empty() && !stmt.isExtern) {
            /// Only export a signature whose C++ representation is
            /// identical whether declared normally or via Extern/Declare -
            /// true for primitives, ZSTRING, Pointer, and a plain-data/
            /// opaque UserDefined type, but NOT for STRING: an ordinary
            /// FUNCTION/SUB compiles a STRING parameter/return to a real
            /// BString (by BYREF default), while an Extern Declare's
            /// ZSTRING would compile to a bare `const char*` - the same
            /// Alias-matched symbol name would then be called with the
            /// wrong argument representation entirely (not just a pointer
            /// difference - BString is a non-trivial class), a real,
            /// silent ABI mismatch rather than a merely cosmetic one. This
            /// is a deliberate scope cut, not an oversight: exporting a
            /// STRING-using procedure safely would need an auto-generated
            /// ZSTRING<->BString marshaling shim at the boundary, not just
            /// a re-declared prototype.
            bool hasStringSignature = stmt.declaredType.kind == TypeKind::StringT;
            for (const Param& p : stmt.params) {
                if (p.type.kind == TypeKind::StringT) hasStringSignature = true;
            }
            if (hasStringSignature) {
                skippedText << "' (not exported: '" << stmt.name
                            << "' uses STRING, which needs a marshaling shim not yet implemented)\n";
                continue;
            }
            bool isFunction = stmt.kind == StmtKind::FunctionDecl;
            declsText << "    Declare " << (isFunction ? "Function " : "Sub ") << stmt.name
                      << " Alias \"" << mangleName(stmt.name) << "\" (";
            for (size_t i = 0; i < stmt.params.size(); ++i) {
                if (i > 0) declsText << ", ";
                const Param& p = stmt.params[i];
                declsText << (p.byRef ? "ByRef " : "ByVal ") << p.name << " AS " << basicTypeName(p.type);
            }
            declsText << ")";
            if (isFunction) declsText << " AS " << basicTypeName(stmt.declaredType);
            declsText << "\n";
        } else if (stmt.kind == StmtKind::Const) {
            /// Only a literal-valued CONST is re-emitted - safe, since
            /// re-declaring a literal crosses no ABI boundary at all
            /// (unlike a call). A CONST initialized from a more general
            /// constant expression (referencing another CONST/ENUM member,
            /// an operator, ...) isn't re-derived here, to avoid silently
            /// emitting the wrong value - skipped instead, same pattern as
            /// the STRING-signature skip above. `tryEvalConstLiteralText`
            /// also recognizes a negated literal (`-3`) - a real, negative-
            /// valued C enum constant (GTK_RESPONSE_ACCEPT) would otherwise
            /// be silently unexportable, since `-3` parses as
            /// UnaryNeg(IntLiteral(3)), not a single literal node.
            std::optional<std::string> literalText;
            if (stmt.expr) {
                literalText = tryEvalConstLiteralText(*stmt.expr, false);
            }
            if (!literalText) {
                skippedText << "' (not exported: '" << stmt.name
                            << "' CONST initializer isn't a plain (optionally negated) "
                               "integer/double literal)\n";
                continue;
            }
            typesText << "CONST " << stmt.name << " = " << *literalText << "\n";
        } else if (stmt.kind == StmtKind::Enum) {
            /// Unlike CONST, every member's value is already fully
            /// resolved by Sema (resolvedValue) regardless of how it was
            /// originally written (an explicit value or auto-increment) -
            /// no literal-only restriction needed here.
            typesText << "ENUM " << stmt.name << "\n";
            for (const EnumMember& member : stmt.enumMembers) {
                typesText << "    " << member.name << " = " << member.resolvedValue << "\n";
            }
            typesText << "END ENUM\n\n";
        }
    }

    std::ostringstream out;
    out << "' Auto-generated interface for library '" << libName << "' - do not edit by hand.\n\n";
    out << typesText.str();
    if (!skippedText.str().empty()) out << skippedText.str() << "\n";
    out << "Extern \"C++\" Lib \"" << libName << "\"\n";
    out << declsText.str();
    out << "End Extern\n";
    return out.str();
}

} // namespace ebasic
