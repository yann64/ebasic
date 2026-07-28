#pragma once

#include <string>
#include <vector>

namespace ebasic {

// Runs args[0] with the given argv (no shell involved), waits for it to
// exit, and returns its exit status (or -1 if it could not be started/waited on).
// Cross-platform (M8a): POSIX (fork/execvp/waitpid) or Windows
// (CreateProcess) under the hood, picked at compile time - see process.cpp.
int runProcess(const std::vector<std::string>& args);

// Same as runProcess, but also captures everything the child writes to
// stdout into `output` (its stderr still goes to this process's own,
// unchanged) - needed by ebpm's git-dependency resolution (M5d) to read
// back a command's result (e.g. `git rev-parse HEAD`), not just its exit
// status.
int runProcessCaptureOutput(const std::vector<std::string>& args, std::string& output);

} // namespace ebasic
