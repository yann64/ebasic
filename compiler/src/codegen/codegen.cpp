#include "codegen/codegen.hpp"

#include <cctype>
#include <stdexcept>

namespace ebasic {

std::string Codegen::mangleName(const std::string& name) {
    std::string r = "eb_";
    for (char c : name) r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return r;
}

std::string Codegen::cppType(TypeKind type) {
    switch (type) {
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

std::string Codegen::genExpr(const Expr& expr) {
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
            std::string key = canonicalName(expr.stringValue);
            auto arrIt = arrayLowerBoundVar_.find(key);
            if (arrIt != arrayLowerBoundVar_.end()) {
                return mangleName(expr.stringValue) + "[static_cast<std::size_t>((" +
                       genExpr(*expr.args[0]) + ") - " + arrIt->second + ")]";
            }
            std::string result = mangleName(expr.stringValue) + "(";
            for (size_t i = 0; i < expr.args.size(); ++i) {
                if (i > 0) result += ", ";
                result += genExpr(*expr.args[i]);
            }
            result += ")";
            return result;
        }
        case ExprKind::UnaryNeg:
            return "(-" + genExpr(*expr.lhs) + ")";
        case ExprKind::UnaryNot:
            return "(~(" + genExpr(*expr.lhs) + "))";
        case ExprKind::Binary: {
            const std::string lhs = genExpr(*expr.lhs);
            const std::string rhs = genExpr(*expr.rhs);

            if (expr.binOp == BinOp::Div && !isFloatFamily(expr.lhs->type) &&
                !isFloatFamily(expr.rhs->type)) {
                // FreeBASIC's '/' is always real division; force it since two
                // C++ integers would otherwise truncate.
                return "(static_cast<double>(" + lhs + ") / static_cast<double>(" + rhs + "))";
            }
            if (expr.binOp == BinOp::Pow) {
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
                // DIM arr() AS Type: a dynamic array, empty until REDIM'd.
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
            // EXIT SUB/FUNCTION need no label/goto: a plain C++ `return`
            // already unwinds out of any number of enclosing loops within
            // the current function, which is exactly what they mean.
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
        case StmtKind::CallStmt: {
            out << ind(indent) << mangleName(stmt.name) << "(";
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
    }
}

void Codegen::genProcedure(const Stmt& stmt) {
    bool isFunction = stmt.kind == StmtKind::FunctionDecl;
    std::string retType = isFunction ? cppType(stmt.declaredType) : "void";
    std::string name = mangleName(stmt.name);

    std::string paramList;
    for (size_t i = 0; i < stmt.params.size(); ++i) {
        if (i > 0) paramList += ", ";
        const Param& p = stmt.params[i];
        paramList += cppType(p.type);
        paramList += p.byRef ? "& " : " ";
        paramList += mangleName(p.name);
    }

    protoOut_ << retType << " " << name << "(" << paramList << ");\n";

    procOut_ << retType << " " << name << "(" << paramList << ") {\n";
    if (isFunction) {
        procOut_ << ind(1) << retType << " eb__ret{};\n";
    }
    // A local array can shadow a global of the same name; save/restore so
    // that mapping doesn't leak into code generated after this procedure.
    auto savedArrayLowerBounds = arrayLowerBoundVar_;
    genBlock(stmt.body, procOut_, 1);
    arrayLowerBoundVar_ = savedArrayLowerBounds;
    if (isFunction) {
        procOut_ << ind(1) << "return eb__ret;\n";
    }
    procOut_ << "}\n\n";
}

std::string Codegen::generate(const Module& module) {
    // Top-level DIM/CONST/ENUM become real C++ globals (declared before any
    // function bodies) so SUB/FUNCTION can see them; SUB/FUNCTION become
    // separate C++ functions (prototype + body); everything else still runs
    // in main(), in its original relative order. Sema already enforces
    // declare-before-use sequentially (including for globals referenced from
    // a procedure body), so bucketing all globals before all procedures is
    // always a safe superset of that ordering.
    std::ostringstream globalsOut;
    std::ostringstream mainOut;

    for (const auto& stmtPtr : module.stmts) {
        const Stmt& stmt = *stmtPtr;
        if (stmt.kind == StmtKind::SubDecl || stmt.kind == StmtKind::FunctionDecl) {
            genProcedure(stmt);
        } else if (stmt.kind == StmtKind::Dim || stmt.kind == StmtKind::Const ||
                   stmt.kind == StmtKind::Enum) {
            genStmt(stmt, globalsOut, 0);
        } else {
            genStmt(stmt, mainOut, 1);
        }
    }

    std::ostringstream out;
    out << "// Generated by ebc. Do not edit.\n";
    out << "#include \"ebasic/runtime/runtime.hpp\"\n";
    out << "#include <cmath>\n";
    out << "#include <cstdint>\n";
    out << "#include <vector>\n\n";
    if (!globalsOut.str().empty()) out << globalsOut.str() << "\n";
    if (!protoOut_.str().empty()) out << protoOut_.str() << "\n";
    out << procOut_.str();
    out << "int main() {\n";
    out << mainOut.str();
    out << "    return 0;\n";
    out << "}\n";
    return out.str();
}

} // namespace ebasic
