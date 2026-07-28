#pragma once

#include <string>
#include <vector>

namespace ebasic {

/// Runs args[0] with the given argv (no shell involved), waits for it to
/// exit, and returns its exit status (or -1 if it could not be started/waited on).
/// Cross-platform (M8a): POSIX (fork/execvp/waitpid) or Windows
/// (CreateProcess) under the hood, picked at compile time - see process.cpp.
int runProcess(const std::vector<std::string>& args);

/// Same as runProcess, but also captures everything the child writes to
/// stdout into `output` (its stderr still goes to this process's own,
/// unchanged) - needed by ebpm's git-dependency resolution (M5d) to read
/// back a command's result (e.g. `git rev-parse HEAD`), not just its exit
/// status.
int runProcessCaptureOutput(const std::vector<std::string>& args, std::string& output);

/// Wraps `args` with `flatpak-spawn --host` when running inside a Flatpak
/// sandbox (detected via /.flatpak-info), a no-op everywhere else. Callers
/// opt into this explicitly for a genuine *host*-side tool (the backend
/// compiler, `ar`, `git`) - never for a call to one of this project's own
/// bundled binaries, which must keep running inside the sandbox. See
/// process.cpp's own doc comment for why this isn't applied automatically.
std::vector<std::string> hostExecArgs(const std::vector<std::string>& args);

} // namespace ebasic
