// Target: Iterative flattening into main
int square(int num) {
    return num * num;
}

int cube(int num) {
    return square(num) * num;
}

int main() {
    int base = 3;
    int result = cube(base);
    return result;
}