int add(int a, int b) {
    return a + b;
}

int mul3(int x) {
    return x * 3;
}

int large_helper(int x) {
    int sum = 0;
    for (int i = 0; i < 100; ++i) {
        sum += (x + i) * (x - i);
        sum ^= (sum >> 1);
    }
    return sum;
}

int recurse(int n) {
    if (n <= 0) {
        return 0;
    }
    return n + recurse(n - 1);
}

int main() {
    int a = add(2, 3);
    int b = mul3(a);
    int c = large_helper(b);
    int d = recurse(4);
    return c + d;
}
