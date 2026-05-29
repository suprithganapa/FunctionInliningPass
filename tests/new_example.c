int inc(int x) {
    return x + 1;
}

int big_fn(int n) {
    int s = 0;
    for (int i = 0; i < 80; ++i) {
        s += (n + i) * (n - i);
    }
    return s;
}

int main() {
    int a = inc(41);      // should inline
    int b = big_fn(a);    // likely skip (too costly)
    return b;
}