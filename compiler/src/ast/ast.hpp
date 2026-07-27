#pragma once

#include "diagnostics/diagnostics.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ebasic {

enum class TypeKind {
    Unknown,
    Integer,
    Double,
    StringT,
};

enum class ExprKind {
    IntLiteral,
    DoubleLiteral,
    StringLiteral,
    Ident,
    Binary,
    UnaryNeg,
};

enum class BinOp {
    Add,
    Sub,
    Mul,
    Div,
    Concat,
};

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
