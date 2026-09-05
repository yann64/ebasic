/** Tiny C fixture library for the M4 (C/C++ interop) e2e tests. Deliberately
 * plain C89-ish code with a stable, simple ABI - real-world proof that ebc's
 * generated code can link against a genuine, separately-compiled C library,
 * not just call into its own runtime. */

#include <stdlib.h>
#include <string.h>

/** Integer addition - exercises basic EXTERN "C" scalar-argument calls. */
int eb_fixture_add(int a, int b) {
    return a + b;
}

/** M8f: real Win32 APIs (User32/GDI/...) are always `__stdcall`, unlike
 * every other function in this file - a no-op qualifier on every non-
 * Windows target (there's only one calling convention there), matching
 * ebc's own EBASIC_STDCALL macro exactly (codegen.cpp's generate()), so
 * both sides of the ABI always agree regardless of platform. */
#if defined(_WIN32)
#define EB_FIXTURE_STDCALL __stdcall
#else
#define EB_FIXTURE_STDCALL
#endif

/** Same shape as eb_fixture_add above, just __stdcall - exercises a
 * Stdcall-declared EXTERN import calling a real __stdcall function
 * correctly (a calling-convention mismatch between caller and callee is a
 * real, silent stack-corruption bug on x86 Windows, not just a style
 * choice - this proves the two sides genuinely agree, not just that both
 * compile). */
int EB_FIXTURE_STDCALL eb_fixture_stdcall_add(int a, int b) {
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

/** Invokes a caller-supplied callback (a GLib GCallback-shaped function
 * pointer: a value plus an opaque user_data pointer) - exercises a real C
 * library calling back into eBasic-compiled code via a function pointer
 * eBasic itself produced (`@ProcName`), the same pattern GTK's own
 * g_signal_connect-family APIs use for every widget signal. */
typedef void (*eb_fixture_callback)(int value, void* user_data);

void eb_fixture_invoke_callback(eb_fixture_callback cb, int value, void* user_data) {
    cb(value, user_data);
}

/** A properly-typed comparator callback (the `qsort`-comparator shape),
 * distinct from eb_fixture_callback above: exercises eBasic's typed
 * function-pointer EXTERN/DECLARE parameters end to end (parse -> sema
 * signature match -> real C function-pointer codegen, not `void*`). */
typedef int (*eb_fixture_comparator)(int a, int b);

int eb_fixture_invoke_comparator(eb_fixture_comparator cmp, int a, int b) {
    return cmp(a, b);
}

/** Same shape as eb_fixture_comparator above, just __stdcall-typed -
 * exercises an eBasic-defined Stdcall callback (a plain top-level
 * FUNCTION marked Stdcall, address-taken via @ProcName) genuinely
 * agreeing on calling convention with a real __stdcall C function
 * pointer type, the same "not just that both compile" proof
 * eb_fixture_stdcall_add already established for a plain EXTERN import. */
typedef int(EB_FIXTURE_STDCALL* eb_fixture_stdcall_comparator)(int a, int b);

int eb_fixture_invoke_stdcall_comparator(eb_fixture_stdcall_comparator cmp, int a, int b) {
    return cmp(a, b);
}

/** Returns a freshly malloc'd copy of a fixed string, as a plain void* (the
 * same shape a real void*-returning C API - g_malloc/g_strdup, glib's
 * gtk_text_buffer_get_text, etc. - hands back) - the caller must free it via
 * eb_fixture_free. Exercises reading it back as ZSTRING (ANY PTR -> ZSTRING
 * bridge) and freeing the original ANY PTR value (no bridge needed there). */
void* eb_fixture_malloc_string(void) {
    const char* text = "malloc'd string";
    char* copy = (char*)malloc(strlen(text) + 1);
    strcpy(copy, text);
    return copy;
}

/** Frees a pointer returned by eb_fixture_malloc_string - a generic
 * void*-only free, matching g_free's own signature. */
void eb_fixture_free(void* p) {
    free(p);
}

/** Sums strlen() of each of `count` strings in an array (count-driven, not
 * NULL-terminated) - reads each string's *real content*, not just pointer
 * non-nullity, unlike a NULL-counting function (glib's g_strv_length) would.
 * Regression test for a real dangling-pointer bug: assigning a string
 * literal to a ZSTRING array element used to wrap it in a temporary BString,
 * whose own operator-const-char* pointer dangled the moment that temporary
 * was destroyed at the end of the assignment statement - invisible for a
 * same-statement call argument, but silently wrong once the array was built
 * across several statements and read back later (found building a
 * GSubprocess argv array in eb-gtk4). */
int eb_fixture_sum_lengths(const char* const* arr, int count) {
    int total = 0;
    int i;
    for (i = 0; i < count; i++) {
        total += (int)strlen(arr[i]);
    }
    return total;
}
