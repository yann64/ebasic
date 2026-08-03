#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>

#include "ebasic/runtime/bstring.hpp"

/// FreeBASIC-style math, numeric-conversion, and base-conversion
/// procedures - eBasic's own standard library, made available in every
/// compiled program (never opt-in) via the same compiler-injected
/// `Extern "C++"` prelude the string/file libraries already use (see
/// compiler/src/preprocessor/builtin_prelude.hpp). Pulled in
/// unconditionally by runtime.hpp - no linking step needed, matching
/// this runtime's own existing header-only design.
namespace ebasic::rt::mathlib {

inline double Abs(double n) { return std::fabs(n); }

/// -1, 0, or 1 - the sign of `n`.
inline std::int32_t Sgn(double n) {
    if (n > 0) return 1;
    if (n < 0) return -1;
    return 0;
}

inline double Sqr(double n) { return std::sqrt(n); }
inline double Sin(double n) { return std::sin(n); }
inline double Cos(double n) { return std::cos(n); }
inline double Tan(double n) { return std::tan(n); }
inline double Asin(double n) { return std::asin(n); }
inline double Acos(double n) { return std::acos(n); }
inline double Atn(double n) { return std::atan(n); }
inline double Atan2(double y, double x) { return std::atan2(y, x); }
inline double Exp(double n) { return std::exp(n); }
inline double Log(double n) { return std::log(n); }

/// Floors toward -infinity: Int(-1.5) = -2 (matching real FreeBASIC's
/// own Int, as distinct from Fix below).
inline double Int(double n) { return std::floor(n); }

/// Truncates toward zero: Fix(-1.5) = -1.
inline double Fix(double n) { return std::trunc(n); }

/// One process-global engine backing Rnd/Randomize - eBasic programs
/// are single-threaded, so a single static instance is sufficient.
inline std::mt19937_64& rngEngine() {
    static std::mt19937_64 engine(std::random_device{}());
    return engine;
}

/// The next random value in [0, 1). Real FreeBASIC's own Rnd(n) has
/// extra legacy quirks for n < 0 (deterministic reseed) and n = 0
/// (repeat the last value) - deliberately not replicated here; `n` is
/// accepted (for call-site compatibility with FreeBASIC source) but
/// otherwise unused.
inline double Rnd(double n) {
    (void)n;
    static std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rngEngine());
}

/// Reseeds the shared RNG - `seed = 0` (the default) seeds from the
/// current time, matching real FreeBASIC's own Randomize with no useful
/// argument; a nonzero seed reseeds deterministically.
inline void Randomize(double seed) {
    if (seed == 0) {
        rngEngine().seed(std::random_device{}());
    } else {
        rngEngine().seed(static_cast<std::uint64_t>(seed));
    }
}

// Explicit numeric conversions - one per distinct eBasic primitive type
// (not FreeBASIC's full historical list, which has several names for
// what are, in eBasic, the same underlying type). Each is a plain
// static_cast that truncates toward zero - unlike real FreeBASIC's own
// CInt/CLng, which round to the nearest integer, these deliberately
// match Fix's truncating behavior instead, for a single simple, uniform
// rule across every Cxxx function.
inline std::int8_t CByte(double n) { return static_cast<std::int8_t>(n); }
inline std::uint8_t CUByte(double n) { return static_cast<std::uint8_t>(n); }
inline std::int16_t CShort(double n) { return static_cast<std::int16_t>(n); }
inline std::uint16_t CUShort(double n) { return static_cast<std::uint16_t>(n); }
inline std::int32_t CInt(double n) { return static_cast<std::int32_t>(n); }
inline std::uint32_t CUInt(double n) { return static_cast<std::uint32_t>(n); }
inline std::int64_t CLngInt(double n) { return static_cast<std::int64_t>(n); }
inline std::uint64_t CULngInt(double n) { return static_cast<std::uint64_t>(n); }
inline float CSng(double n) { return static_cast<float>(n); }
inline double CDbl(double n) { return n; }

/// BOOLEAN maps to std::int8_t with TRUE = -1 (this language's own
/// convention) - nonzero converts to -1, not 1.
inline std::int8_t CBool(double n) { return n != 0 ? -1 : 0; }

namespace detail {

inline BString toBase(std::int64_t n, int base, const char* digits) {
    std::uint64_t un = static_cast<std::uint64_t>(n);
    if (un == 0) return BString("0");
    std::string result;
    while (un != 0) {
        result.push_back(digits[un % static_cast<std::uint64_t>(base)]);
        un /= static_cast<std::uint64_t>(base);
    }
    std::reverse(result.begin(), result.end());
    return BString(result);
}

} // namespace detail

/// Uppercase hex, matching real FreeBASIC's own Hex$.
inline BString Hex(std::int64_t n) { return detail::toBase(n, 16, "0123456789ABCDEF"); }
inline BString Oct(std::int64_t n) { return detail::toBase(n, 8, "01234567"); }
inline BString Bin(std::int64_t n) { return detail::toBase(n, 2, "01"); }

} // namespace ebasic::rt::mathlib

// Thin global forwarders - the compiler's own builtin prelude declares
// these exact bare names via `Extern "C++"` with no `Namespace` block
// (so they're callable unqualified from any .bas program), which means
// the real, linkable C++ symbol must be a plain top-level function of
// that same name - the actual logic stays namespaced above.
inline double Abs(double n) { return ::ebasic::rt::mathlib::Abs(n); }
inline std::int32_t Sgn(double n) { return ::ebasic::rt::mathlib::Sgn(n); }
inline double Sqr(double n) { return ::ebasic::rt::mathlib::Sqr(n); }
inline double Sin(double n) { return ::ebasic::rt::mathlib::Sin(n); }
inline double Cos(double n) { return ::ebasic::rt::mathlib::Cos(n); }
inline double Tan(double n) { return ::ebasic::rt::mathlib::Tan(n); }
inline double Asin(double n) { return ::ebasic::rt::mathlib::Asin(n); }
inline double Acos(double n) { return ::ebasic::rt::mathlib::Acos(n); }
inline double Atn(double n) { return ::ebasic::rt::mathlib::Atn(n); }
inline double Atan2(double y, double x) { return ::ebasic::rt::mathlib::Atan2(y, x); }
inline double Exp(double n) { return ::ebasic::rt::mathlib::Exp(n); }
inline double Log(double n) { return ::ebasic::rt::mathlib::Log(n); }
inline double Int(double n) { return ::ebasic::rt::mathlib::Int(n); }
inline double Fix(double n) { return ::ebasic::rt::mathlib::Fix(n); }
inline double Rnd(double n) { return ::ebasic::rt::mathlib::Rnd(n); }
inline void Randomize(double seed) { ::ebasic::rt::mathlib::Randomize(seed); }
inline std::int8_t CByte(double n) { return ::ebasic::rt::mathlib::CByte(n); }
inline std::uint8_t CUByte(double n) { return ::ebasic::rt::mathlib::CUByte(n); }
inline std::int16_t CShort(double n) { return ::ebasic::rt::mathlib::CShort(n); }
inline std::uint16_t CUShort(double n) { return ::ebasic::rt::mathlib::CUShort(n); }
inline std::int32_t CInt(double n) { return ::ebasic::rt::mathlib::CInt(n); }
inline std::uint32_t CUInt(double n) { return ::ebasic::rt::mathlib::CUInt(n); }
inline std::int64_t CLngInt(double n) { return ::ebasic::rt::mathlib::CLngInt(n); }
inline std::uint64_t CULngInt(double n) { return ::ebasic::rt::mathlib::CULngInt(n); }
inline float CSng(double n) { return ::ebasic::rt::mathlib::CSng(n); }
inline double CDbl(double n) { return ::ebasic::rt::mathlib::CDbl(n); }
inline std::int8_t CBool(double n) { return ::ebasic::rt::mathlib::CBool(n); }
inline ::ebasic::rt::BString Hex(std::int64_t n) { return ::ebasic::rt::mathlib::Hex(n); }
inline ::ebasic::rt::BString Oct(std::int64_t n) { return ::ebasic::rt::mathlib::Oct(n); }
inline ::ebasic::rt::BString Bin(std::int64_t n) { return ::ebasic::rt::mathlib::Bin(n); }
