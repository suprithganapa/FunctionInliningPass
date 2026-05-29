int heavy_compute(int x) {
    int acc = 0;
    for (int i = 0; i < 200; ++i) {
        acc += (x + i) * (x - i);
        acc ^= (acc << 1);
        acc += i;
    }
    return acc;
}

int main() {
    return heavy_compute(9);
}
