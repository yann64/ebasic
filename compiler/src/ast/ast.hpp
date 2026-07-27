#pragma once

#include "diagnostics/diagnostics.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ebasic {

enum class TypeKind {
    Unknown,
    Byte,
    UByte,
    Short,
    UShort,
    Integer,
    Long,
    UInteger,
    LongInt,
    ULongInt,
    Single,
    Double,
    Boolean,
    StringT,
};

enum class ExprKind {
    IntLiteral,
    DoubleLiteral,
    StringLiteral,
    BoolLiteral,
    Ident,
    Binary,
    UnaryNeg,
    UnaryNot,
};

enum class BinOp {
    Add,
    Sub,
    Mul,
    Div,
    IDiv,
    Mod,
    Pow,
    Shl,
    Shr,
    Concat,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
    Xor,
};

// Integer-family types, including Boolean (stored as -1/0, so bitwise ops on
// it double as logical ops, matching FreeBASIC's AND/OR/XOR/NOT semantics).
inline bool isIntegerFamily(TypeKind t) {
    switch (t) {
        case TypeKind::Byte:
        case TypeKind::UByte:
        case TypeKind::Short:
        case TypeKind::UShort:
        case TypeKind::Integer:
        case TypeKind::Long:
        case TypeKind::UInteger:
        case TypeKind::LongInt:
        case TypeKind::ULongInt:
        case TypeKind::Boolean:
            return true;
        default:
            return false;
    }
}

inline bool isFloatFamily(TypeKind t) {
    return t == TypeKind::Single || t == TypeKind::Double;
}

inline bool isNumericType(TypeKind t) {
    return isIntegerFamily(t) || isFloatFamily(t);
}

inline bool isUnsignedType(TypeKind t) {
    switch (t) {
        case TypeKind::UByte:
        case TypeKind::UShort:
        case TypeKind::UInteger:
        case TypeKind::ULongInt:
            return true;
        default:
            return false;
    }
}

// Byte width class used to pick the wider of two integer-family types when
// promoting a binary operation's operands. Boolean ranks with Byte (both are
// 1-byte types at the C++ level).
inline int integerRank(TypeKind t) {
    switch (t) {
        case TypeKind::Byte:
        case TypeKind::UByte:
        case TypeKind::Boolean:
            return 1;
        case TypeKind::Short:
        case TypeKind::UShort:
            return 2;
        case TypeKind::Integer:
        case TypeKind::Long:
        case TypeKind::UInteger:
            return 4;
        case TypeKind::LongInt:
        case TypeKind::ULongInt:
            return 8;
        default:
            return 0;
    }
}

// Result type of combining two integer-family operands (arithmetic/bitwise).
// Wider rank wins; same rank with mixed signedness promotes to the unsigned
// member of the pair, matching C++'s own usual arithmetic conversions.
inline TypeKind promoteInteger(TypeKind a, TypeKind b) {
    int ra = integerRank(a);
    int rb = integerRank(b);
    if (ra != rb) return ra > rb ? a : b;
    if (isUnsignedType(a) != isUnsignedType(b)) return isUnsignedType(a) ? a : b;
    return a;
}

// Result type of combining two float-family operands: Double wins over Single.
inline TypeKind promoteFloat(TypeKind a, TypeKind b) {
    return (a == TypeKind::Double || b == TypeKind::Double) ? TypeKind::Double : TypeKind::Single;
}

// General numeric promotion for arithmetic/bitwise binary operators: a float
// operand always wins (keeping its own width) over an integer operand.
// Caller must ensure both types are numeric.
inline TypeKind promoteNumeric(TypeKind a, TypeKind b) {
    bool fa = isFloatFamily(a);
    bool fb = isFloatFamily(b);
    if (fa && fb) return promoteFloat(a, b);
    if (fa) return a;
    if (fb) return b;
    return promoteInteger(a, b);
}

struct Expr {
    ExprKind kind;
    SourceLoc loc;
    TypeKind type = TypeKind::Unknown;

    long long intValue = 0;
    double doubleValue = 0.0;
    std::string stringValue; // StringLiteral text, or Ident name
    BinOp binOp = BinOp::Add;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
};

using ExprPtr = std::unique_ptr<Expr>;

enum class StmtKind {
    Dim,
    Assign,
    Print,
};

struct Stmt {
    StmtKind kind;
    SourceLoc loc;

    std::string name;                        // Dim, Assign
    TypeKind declaredType = TypeKind::Unknown; // Dim
    ExprPtr expr;                             // Assign
    std::vector<ExprPtr> args;                // Print
};

using StmtPtr = std::unique_ptr<Stmt>;

struct Module {
    std::vector<StmtPtr> stmts;
};

} // namespace ebasic
