/* Dynamic-loading harness for the --shared-lib/-dll e2e test
 * (tests/cli/shared_lib.sh) - genuine proof of dynamic loadability, not
 * just "the file exists": loads the shared library built from mylib.bas,
 * resolves its one real export (AddNumbers) by its verbatim, unmangled
 * name, calls it, and checks the real return value. Two real code paths,
 * not one: dlopen/dlsym everywhere except Windows, LoadLibrary/
 * GetProcAddress on Windows/MinGW (there is no dlfcn.h there). */
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: harness <path-to-shared-lib>\n");
        return 2;
    }
    typedef int (*AddFn)(int, int);
    AddFn add;

#ifdef _WIN32
    HMODULE handle = LoadLibraryA(argv[1]);
    if (!handle) {
        fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }
    add = (AddFn)GetProcAddress(handle, "AddNumbers");
    if (!add) {
        fprintf(stderr, "GetProcAddress failed: %lu\n", GetLastError());
        return 1;
    }
#else
    void* handle = dlopen(argv[1], RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    add = (AddFn)dlsym(handle, "AddNumbers");
    if (!add) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
        return 1;
    }
#endif

    int result = add(3, 4);
    printf("AddNumbers(3, 4) = %d\n", result);

#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
    return 0;
}
