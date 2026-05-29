# EVALUATION.md — FunctionInliningPass

## Methodology

Each test case is a C source file in `tests/`. The evaluation pipeline is:

1. Compile to LLVM IR: `clang -S -emit-llvm tests/<file>.c -o ir/<file>.ll`
2. Run the pass: `opt -load-pass-plugin ... -passes="simple-inline-pass" ir/<file>.ll -S -o optimized/<file>.optimized.ll`
3. Capture pass log: stderr is redirected to `optimized/<file>.log`
4. Compare IR: `diff ir/<file>.ll optimized/<file>.optimized.ll`

**Threshold used in all evaluations:** `10` (default)

---

## Test Cases

### Test 1: `small.c` — Basic Inlining (Pass Case)

**Source:**
```c
int add(int a, int b) { return a + b; }

int main() {
    int x = 2, y = 3;
    int z = add(x, y);
    return z;
}
```

**Expected:** `add` is inlined into `main`; `add` is removed as dead.

**Pass log:**
```
[simple-inline-pass] Function: add | Cost: 8 | INLINE
[simple-inline-pass] Inlined function: add
[simple-inline-pass] Removing dead function: add
[simple-inline-pass] Calls inspected: 1 | Calls inlined: 1 | Dead functions removed: 1
```

**Result:** ✅ PASS — `call i32 @add` eliminated in optimized IR; `add` definition removed.

---

### Test 2: `large.c` — Cost Threshold (Skip Case)

**Source:** A function with a large loop body (many instructions).

**Expected:** Function cost exceeds threshold → skipped.

**Pass log:**
```
[simple-inline-pass] Function: large_helper | Cost: 41 | SKIP
[simple-inline-pass] Calls inspected: 1 | Calls inlined: 0 | Cost-based skips: 1
```

**Result:** ✅ PASS — Large function correctly skipped; IR unchanged.

---

### Test 3: `recursive.c` — Recursion Guard (Failure Case)

**Source:**
```c
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}
int main() { return fact(5); }
```

**Expected:** `fact` is directly recursive → must be skipped to avoid infinite expansion.

**Pass log:**
```
[simple-inline-pass] Skipping recursive function: fact
[simple-inline-pass] Function: fact | Cost: 22 | SKIP
[simple-inline-pass] Calls inspected: 1 | Calls inlined: 0 | Recursive blocks: 1
```

**Result:** ✅ PASS (failure case correctly handled) — Recursion blocked; IR unchanged; no infinite loop.

---

### Test 4: `mixed.c` — Mixed Inlining

**Source:** Contains `add` (small), `mul3` (small), `recurse` (recursive), `large_helper` (large).

**Expected:** `add` and `mul3` inlined; `recurse` and `large_helper` skipped.

**Pass log:**
```
[simple-inline-pass] Skipping recursive function: recurse
[simple-inline-pass] Function: recurse | Cost: 22 | SKIP
[simple-inline-pass] Function: add | Cost: 8 | INLINE
[simple-inline-pass] Inlined function: add
[simple-inline-pass] Function: mul3 | Cost: 5 | INLINE
[simple-inline-pass] Inlined function: mul3
[simple-inline-pass] Function: large_helper | Cost: 41 | SKIP
[simple-inline-pass] Removing dead function: add
[simple-inline-pass] Removing dead function: mul3
[simple-inline-pass] Calls inspected: 5 | Calls inlined: 2 | Recursive blocks: 2 | Cost-based skips: 1 | Dead functions removed: 2
```

**Result:** ✅ PASS — Selective inlining works correctly; all four rules exercised in one test.

---

### Test 5: `new_example.c` — Threshold Boundary

**Source:**
```c
int inc(int x) { return x + 1; }

int big_fn(int n) {
    int s = 0;
    for (int i = 0; i < 80; ++i) s += (n + i) * (n - i);
    return s;
}

int main() {
    int a = inc(41);
    int b = big_fn(a);
    return b;
}
```

**Expected:** `inc` inlined (low cost); `big_fn` skipped (loop back-edge triggers +5 bonus, pushing cost over threshold).

**Pass log:**
```
[simple-inline-pass] Function: inc | Cost: 3 | INLINE
[simple-inline-pass] Inlined function: inc
[simple-inline-pass] Function: big_fn | Cost: 27 | SKIP
[simple-inline-pass] Removing dead function: inc
[simple-inline-pass] Calls inspected: 2 | Calls inlined: 1 | Cost-based skips: 1 | Dead functions removed: 1
```

**Result:** ✅ PASS — Threshold boundary case handled; loop detection via back-edge heuristic works.

---

## Summary Table

| Test | Scenario | Calls Inspected | Inlined | Skipped | Dead Removed | Result |
|------|----------|-----------------|---------|---------|--------------|--------|
| `small` | Basic inlining | 1 | 1 | 0 | 1 | ✅ PASS |
| `large` | Cost too high | 1 | 0 | 1 | 0 | ✅ PASS |
| `recursive` | Direct recursion | 1 | 0 | 1 | 0 | ✅ PASS |
| `mixed` | Mixed cases | 5 | 2 | 3 | 2 | ✅ PASS |
| `new_example` | Threshold boundary | 2 | 1 | 1 | 1 | ✅ PASS |

---

## Baseline Comparison

**Baseline:** Running `opt` with no pass (`opt -S ir/small.ll -o baseline.ll`) — IR is unchanged.

**With pass:** The `call i32 @add(...)` instruction is eliminated and replaced with inline arithmetic. The `add` function definition is removed entirely from the module.

**IR diff for `small.c`:**
```diff
- %call = call i32 @add(i32 %0, i32 %1)
+ %add = add nsw i32 %0, %1

- define dso_local i32 @add(i32 noundef %a, i32 noundef %b) {
-   ...
- }
```

This demonstrates that the pass successfully eliminates function call overhead for eligible callees.

---

## Metrics

| Metric | Value |
|--------|-------|
| Total test cases | 5 |
| Passing | 5 |
| Failing | 0 |
| Inlining correctly triggered | Yes |
| Recursion guard working | Yes |
| Cost threshold enforced | Yes |
| Dead function cleanup working | Yes |
| Unreachable block cleanup | Yes (triggered post-inlining where applicable) |
