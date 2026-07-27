#pragma once

#include <string>
#include <vector>

namespace ebasic {

// Runs args[0] with the given argv (no shell involved), waits for it to
// exit, and returns its exit status (or -1 if it could not be started/waited on).
int runProcess(const std::vector<std::string>& args);

} // namespace ebasic
