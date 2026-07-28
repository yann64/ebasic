#include "driver/process.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ebasic {

#ifdef _WIN32

namespace {

// Windows has no execvp-style "argv array" process-creation API -
// CreateProcess takes a single command-line *string*, which the child's
// own CRT re-splits back into argv using this exact convention. Building
// it by hand means following the documented Windows argument-quoting
// algorithm precisely (Microsoft's own reference implementation, widely
// cited as "the" correct way to do this): an argument with no
// space/tab/newline/quote is passed through unquoted; otherwise it's
// wrapped in quotes, with every run of backslashes doubled when it
// immediately precedes either a literal quote (which itself gets escaped)
// or the end of the argument (so the closing quote we add isn't itself
// escaped). Getting this wrong is a classic, subtle bug (arguments
// containing spaces or quotes silently splitting wrong) - written and
// reviewed carefully since there's no local Windows machine to test it
// against; correctness here is verified via CI (M8d), not assumed.
std::string quoteArgvArgument(const std::string& argument) {
    if (!argument.empty() && argument.find_first_of(" \t\n\v\"") == std::string::npos) {
        return argument;
    }
    std::string result = "\"";
    for (auto it = argument.begin();; ++it) {
        unsigned backslashes = 0;
        while (it != argument.end() && *it == '\\') {
            ++it;
            ++backslashes;
        }
        if (it == argument.end()) {
            result.append(backslashes * 2, '\\');
            break;
        } else if (*it == '"') {
            result.append(backslashes * 2 + 1, '\\');
            result.push_back('"');
        } else {
            result.append(backslashes, '\\');
            result.push_back(*it);
        }
    }
    result.push_back('"');
    return result;
}

std::string buildCommandLine(const std::vector<std::string>& args) {
    std::string cmdLine;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmdLine.push_back(' ');
        cmdLine += quoteArgvArgument(args[i]);
    }
    return cmdLine;
}

} // namespace

int runProcess(const std::vector<std::string>& args) {
    std::string cmdLine = buildCommandLine(args);
    // CreateProcessA requires a mutable buffer for its command-line
    // argument (it may rewrite it in place) - a std::string's own buffer
    // isn't guaranteed safe to hand over that way, so it's copied into an
    // explicit, writable buffer first.
    std::vector<char> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back('\0');

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmdLineBuf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                         &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
}

int runProcessCaptureOutput(const std::vector<std::string>& args, std::string& output) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return -1;
    // The parent's (read) end must not be inherited by the child, or the
    // pipe would never signal EOF once the child exits (the child's own
    // stray copy of the read handle would keep it open).
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::string cmdLine = buildCommandLine(args);
    std::vector<char> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back('\0');

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessA(nullptr, cmdLineBuf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
                              &si, &pi);
    // The parent's own copy of the write end must be closed regardless of
    // whether CreateProcess succeeded - the child gets its own inherited
    // copy, and holding this one open would itself prevent EOF.
    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        return -1;
    }

    char buf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
        output.append(buf, bytesRead);
    }
    CloseHandle(readPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
}

#else

int runProcess(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

int runProcessCaptureOutput(const std::vector<std::string>& args, std::string& output) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(pipefd[1]);
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        output.append(buf, static_cast<size_t>(n));
    }
    close(pipefd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

#endif

} // namespace ebasic
