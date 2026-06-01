// Target: SKIP due to Cost > 15 (Size Constraint)
int complex_math(int a, int b) {
    int x = a + b;
    int y = a - b;
    int z = x * y;
    int w = z / 2;
    if (w > 10) {
        w = w * 3;
    } else {
        w = w + 7;
    }
    return w + x - y * z;
}

int main() {
    return complex_math(20, 10);
}