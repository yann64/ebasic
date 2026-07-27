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

void Codegen::genStmt(const Stmt& stmt, std::ostringstream& out) {
    switch (stmt.kind) {
        case StmtKind::Dim:
            out << "    " << cppType(stmt.declaredType) << " " << mangleName(stmt.name) << "{};\n";
            return;
        case StmtKind::Assign:
            out << "    " << mangleName(stmt.name) << " = " << genExpr(*stmt.expr) << ";\n";
            return;
        case StmtKind::Print: {
            out << "    ::ebasic::rt::printLine(";
            for (size_t i = 0; i < stmt.args.size(); ++i) {
                if (i > 0) out << ", ";
                out << genExpr(*stmt.args[i]);
            }
            out << ");\n";
            return;
        }
    }
}

std::string Codegen::generate(const Module& module) {
    std::ostringstream out;
    out << "// Generated by ebc. Do not edit.\n";
    out << "#include \"ebasic/runtime/runtime.hpp\"\n";
    out << "#include <cmath>\n";
    out << "#include <cstdint>\n\n";
    out << "int main() {\n";
    for (const auto& stmt : module.stmts) {
        genStmt(*stmt, out);
    }
    out << "    return 0;\n";
    out << "}\n";
    return out.str();
}

} // namespace ebasic
