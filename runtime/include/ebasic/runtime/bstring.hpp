#pragma once

#include <ostream>
#include <string>
#include <utility>

namespace ebasic::rt {

/// BASIC's dynamic STRING type. Currently an always-copy value type wrapping
/// std::string; the ref-counted/COW representation described in the project
/// roadmap is deferred until real string-heavy programs need it.
class BString {
public:
    BString() = default;
    /// Null-safe: constructing std::string from nullptr is UB, and a null
    /// C string is a real, expected value at an M4 EXTERN/ZSTRING boundary
    /// (e.g. a C function documented to return NULL on failure) - treated
    /// as an empty BASIC string rather than crashing.
    BString(const char* s) : data_(s ? s : "") {}
    BString(std::string s) : data_(std::move(s)) {}

    /// Implicit conversion to a C string, for passing a STRING-typed
    /// argument to a ZSTRING/ZSTRING PTR parameter (M4) - FreeBASIC's own
    /// documented rule is that any string type argument may be passed
    /// directly to a `zstring ptr` parameter. Letting C++'s own
    /// copy-initialization rules perform this conversion at the call site
    /// means Codegen needs no per-argument marshaling logic at all.
    operator const char*() const { return data_.c_str(); }

    BString operator+(const BString& other) const {
        return BString(data_ + other.data_);
    }

    bool operator==(const BString& other) const { return data_ == other.data_; }
    bool operator!=(const BString& other) const { return !(*this == other); }
    bool operator<(const BString& other) const { return data_ < other.data_; }
    bool operator>(const BString& other) const { return data_ > other.data_; }
    bool operator<=(const BString& other) const { return data_ <= other.data_; }
    bool operator>=(const BString& other) const { return data_ >= other.data_; }

    const std::string& str() const { return data_; }

    friend std::ostream& operator<<(std::ostream& os, const BString& s) {
        return os << s.data_;
    }

private:
    std::string data_;
};

} // namespace ebasic::rt
