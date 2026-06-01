// Target: Erase orphan_func immediately during DCE sweep
int orphan_func(int a) {
    return a * 99;
}

int active_func(int b) {
    return b + 2;
}

int main() {
    return active_func(10);
}