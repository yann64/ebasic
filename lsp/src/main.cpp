#include "server.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <iostream>

int main() {
#ifdef _WIN32
    // LSP framing counts exact bytes (Content-Length) - Windows' default
    // text-mode stdio would otherwise translate \n <-> \r\n and corrupt
    // that count.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    ebasic::lsp::Server server;
    return server.run(std::cin, std::cout);
}
