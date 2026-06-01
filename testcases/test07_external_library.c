#include <stdio.h>
#include <stdlib.h>

// Target: SKIP declarations gracefully (Cost 0, No body available)
int main() {
    int *ptr = (int *)malloc(sizeof(int));
    if (ptr == NULL) {
        printf("Allocation failed\n");
        return 1;
    }
    *ptr = 42;
    printf("Value: %d\n", *ptr);
    free(ptr);
    return 0;
}