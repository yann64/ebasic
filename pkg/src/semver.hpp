#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ebpm {

/// A strict, published-version SemVer triple - MAJOR.MINOR.PATCH only, no
/// pre-release/build-metadata (deliberately simpler than the full SemVer
/// 2.0 spec - a stated scope cut of the registry design, not an oversight).
/// Comparable via the usual operators (component-wise, major first).
struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

bool operator==(const SemVer& a, const SemVer& b);
bool operator<(const SemVer& a, const SemVer& b);
inline bool operator!=(const SemVer& a, const SemVer& b) { return !(a == b); }
inline bool operator>(const SemVer& a, const SemVer& b) { return b < a; }
inline bool operator<=(const SemVer& a, const SemVer& b) { return !(b < a); }
inline bool operator>=(const SemVer& a, const SemVer& b) { return !(a < b); }

/// Parses a strict "MAJOR.MINOR.PATCH" string - all three components
/// required, each a non-negative integer with no leading zeros (e.g. "01"
/// is rejected) - the form every *published* index version must take. Use
/// parseVersionReq (below) for a manifest's own, possibly-partial
/// requirement string instead.
bool parseSemVer(const std::string& text, SemVer& out, std::string& err);

std::string toString(const SemVer& v);

/// A manifest dependency's version *requirement* - distinct from SemVer
/// above because a requirement string may give only 1 or 2 of its 3
/// components (`^1`, `~1.2`), which changes what it means (see Op's own
/// doc comment). `precision` records how many components were actually
/// given (1, 2, or 3) - needed to compute the right upper bound for
/// Caret/Tilde, since e.g. `^1` and `^1.2.3` have different upper bounds
/// despite both having major == 1.
struct VersionReq {
    /// Caret (default - a bare "1.2.3" means the same as "^1.2.3") allows
    /// any version compatible per SemVer's "don't break the public API"
    /// convention: the first nonzero component, scanning major -> minor ->
    /// patch, may not change (so `^0.2.3` is narrower than `^1.2.3` - a 0.x
    /// minor bump is considered breaking). Tilde only allows the last
    /// *given* component to vary. Exact matches one literal version only -
    /// requires all 3 components (a partial `=1.2` is ambiguous and
    /// rejected, matching Cargo's own restriction).
    enum class Op { Caret, Tilde, Exact };
    Op op = Op::Caret;
    int major = 0;
    int minor = 0;
    int patch = 0;
    int precision = 1; ///< how many of major/minor/patch were actually given (1-3)
};

/// Parses a requirement string: an optional leading `^`/`~`/`=` (Caret if
/// none given), then 1-3 dot-separated non-negative integers.
bool parseVersionReq(const std::string& text, VersionReq& out, std::string& err);

/// True if `v` satisfies `req`, per the exact Caret/Tilde/Exact rules
/// documented on `VersionReq::Op` above.
bool matches(const VersionReq& req, const SemVer& v);

/// The highest version in `available` that matches `req`, or nullopt if
/// none does.
std::optional<SemVer> pickBestSatisfying(const std::vector<SemVer>& available, const VersionReq& req);

} // namespace ebpm
