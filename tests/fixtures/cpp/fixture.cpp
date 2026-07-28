// Tiny C++ fixture library for the M4 (C/C++ interop) e2e tests. A
// namespaced free function and an overloaded pair, both compiled with real
// C++ linkage (no extern "C") - proof that ebc's Extern "C++" bindings
// resolve against genuine C++ name mangling via the real linker, not an
// emulation of it.

namespace ebfixture {

int Square(int x) {
    return x * x;
}

int Add(int a, int b) {
    return a + b;
}

double Add(double a, double b) {
    return a + b;
}

} // namespace ebfixture
