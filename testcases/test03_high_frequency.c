// Target: Negative net cost due to call frequency
int internal_scale(int val) {
    return val * 5;
}

int main() {
    int sum = 0;
    sum += internal_scale(1);
    sum += internal_scale(2);
    sum += internal_scale(3);
    sum += internal_scale(4);
    sum += internal_scale(5);
    return sum;
}