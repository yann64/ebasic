#pragma once

#include <ostream>
#include <string>
#include <utility>

namespace ebasic::rt {

// BASIC's dynamic STRING type. Currently an always-copy value type wrapping
// std::string; the ref-counted/COW representation described in the project
// roadmap is deferred until real string-heavy programs need it.
class BString {
public:
    BString() = default;
    BString(const char* s) : data_(s) {}
    BString(std::string s) : data_(std::move(s)) {}

    BString operator+(const BString& other) const {
        return BString(data_ + other.data_);
    }

    bool operator==(const BString& other) const { return data_ == other.data_; }
    bool operator!=(const BString& other) const { return !(*this == other); }

    const std::string& str() const { return data_; }

    friend std::ostream& operator<<(std::ostream& os, const BString& s) {
        return os << s.data_;
    }

private:
    std::string data_;
};

} // namespace ebasic::rt
