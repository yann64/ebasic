#pragma once

#include <iostream>

namespace ebasic::rt {

template <typename... Args>
void printLine(const Args&... args) {
    (std::cout << ... << args);
    std::cout << "\n";
}

} // namespace ebasic::rt
