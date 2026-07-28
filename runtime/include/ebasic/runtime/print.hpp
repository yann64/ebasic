#pragma once

#include <cstdint>
#include <iostream>
#include <type_traits>

namespace ebasic::rt {

/// std::int8_t/std::uint8_t (used for BYTE/UBYTE/BOOLEAN) are character types
/// as far as operator<< is concerned, so print them as numbers explicitly.
template <typename T>
void printArg(std::ostream& os, const T& v) {
    if constexpr (std::is_same_v<T, std::int8_t>) {
        os << static_cast<int>(v);
    } else if constexpr (std::is_same_v<T, std::uint8_t>) {
        os << static_cast<unsigned int>(v);
    } else {
        os << v;
    }
}

/// Codegen's lowering target for PRINT - each argument is written via
/// printArg (so BYTE/UBYTE print as numbers, not characters), then a single
/// trailing newline, matching BASIC's own PRINT semantics.
template <typename... Args>
void printLine(const Args&... args) {
    (printArg(std::cout, args), ...);
    std::cout << "\n";
}

} // namespace ebasic::rt
