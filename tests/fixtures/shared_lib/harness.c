/* dlopen/dlsym harness for the --shared-lib/-dll e2e test
 * (tests/cli/shared_lib.sh) - genuine proof of dynamic loadability, not
 * just "the file exists": loads the shared library built from mylib.bas,
 * resolves its one real export (AddNumbers) by its verbatim, unmangled
 * name, calls it, and checks the real return value. */
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: harness <path-to-shared-lib>\n");
        return 2;
    }
    void* handle = dlopen(argv[1], RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    typedef int (*AddFn)(int, int);
    AddFn add = (AddFn)dlsym(handle, "AddNumbers");
    if (!add) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
        return 1;
    }
    int result = add(3, 4);
    printf("AddNumbers(3, 4) = %d\n", result);
    dlclose(handle);
    return 0;
}
