#include <stdio.h>

int pure_leaf(int x) {
    return x * 10;
}

int medium_wrapper(int y) {
    return pure_leaf(y) + 5;
}

int heavy_bloat(int z) {
    if (z > 100) return printf("High\n");
    if (z > 50)  return printf("Mid\n");
    return printf("Low: %d\n", pure_leaf(z));
}

int safe_recursive(int n) {
    if (n <= 1) return 1;
    return n + safe_recursive(n - 1);
}

int main() {
    int input = 5;
    int val = medium_wrapper(input);
    int freq = pure_leaf(val) + pure_leaf(val) + pure_leaf(val);
    heavy_bloat(freq);
    int rec = safe_recursive(5);
    printf("Pipeline terminal state: %d\n", rec);
    return 0;
}