/* Build-time link test for the --shared-lib/-dll e2e test, Windows/MinGW
 * only (tests/cli/shared_lib.sh): links directly against the generated
 * MinGW import library (lib<name>.dll.a) the way a real consuming
 * program would, rather than loading the DLL dynamically at runtime -
 * proves the import library itself is genuinely usable at link time, not
 * just that the DLL exists on disk. */
#include <stdio.h>

extern int AddNumbers(int a, int b);

int main(void) {
    printf("AddNumbers(3, 4) = %d\n", AddNumbers(3, 4));
    return 0;
}
