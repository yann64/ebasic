/** Tiny C fixture library for the M4 (C/C++ interop) e2e tests. Deliberately
 * plain C89-ish code with a stable, simple ABI - real-world proof that ebc's
 * generated code can link against a genuine, separately-compiled C library,
 * not just call into its own runtime. */

#include <stdlib.h>

/** Integer addition - exercises basic EXTERN "C" scalar-argument calls. */
int eb_fixture_add(int a, int b) {
    return a + b;
}

/** Returns a fixed C string - exercises `const char*` return marshaling. */
const char* eb_fixture_greeting(void) {
    return "hello from C";
}

/** Hand-rolled strlen equivalent - exercises passing a C string in. */
int eb_fixture_strlen_like(const char* s) {
    int n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/** Returns NULL when flag == 0 - exercises the null-return marshaling case. */
const char* eb_fixture_maybe_null(int flag) {
    if (flag) {
        return "not null";
    }
    return (const char*)0;
}

/** Opaque "handle" object-style API, matching how many real C libraries
 * (sqlite3, SDL, ...) expose objects: only ever seen through a pointer, the
 * caller never knows or needs the struct's real layout. */
struct eb_fixture_handle {
    int value;
};

/** Allocates a handle - the caller owns it and must eb_fixture_handle_destroy
 * it. */
struct eb_fixture_handle* eb_fixture_handle_create(int initial) {
    struct eb_fixture_handle* h = (struct eb_fixture_handle*)malloc(sizeof(struct eb_fixture_handle));
    h->value = initial;
    return h;
}

/** Reads the handle's current value. */
int eb_fixture_handle_get(struct eb_fixture_handle* h) {
    return h->value;
}

/** Mutates the handle in place - exercises a call with real side effects. */
void eb_fixture_handle_add(struct eb_fixture_handle* h, int delta) {
    h->value += delta;
}

/** Frees a handle created by eb_fixture_handle_create. */
void eb_fixture_handle_destroy(struct eb_fixture_handle* h) {
    free(h);
}
