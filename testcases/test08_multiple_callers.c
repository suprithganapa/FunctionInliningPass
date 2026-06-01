// Target: Verify CallGraph accounts for discrete parent references
int clamp_zero(int val) {
    return (val < 0) ? 0 : val;
}

int process_alpha(int x) {
    return clamp_zero(x - 10);
}

int process_beta(int y) {
    return clamp_zero(y + 5);
}

int main() {
    int res1 = process_alpha(5);
    int res2 = process_beta(-20);
    return res1 + res2;
}