# FunctionInliningPass — LLVM Custom Optimization Pass


> **Compiler Design Lab — Experiential Learning (EL)**  
> Department of Computer Science & Engineering, R V College of Engineering, Bengaluru

## Team
| Name | USN |
|------|-----|
| Suprith GB | 1RV23CS255 |
| Sushanth Joshi | — |
| Tallam Sri Sai Subramanyam | — |

An LLVM New Pass Manager plugin that implements a **custom module-level function inlining optimization** with cost-based heuristics, recursion detection, and dead function elimination.

**Pipeline name:** `simple-inline-pass`  
**Registered via:** `opt -load-pass-plugin`

---

## What It Does

This pass scans all call sites in an LLVM IR module and inlines small, non-recursive helper functions directly into their callers. After inlining, it cleans up unreachable basic blocks and removes functions that are no longer called (dead functions).

Key optimizations performed:
- **Cost-based function inlining** — inlines callees below a configurable instruction-cost threshold
- **Direct recursion blocking** — skips any function that calls itself
- **Dead function elimination** — removes functions with no remaining callers
- **Unreachable block removal** — cleans up CFG after inlining

---

## Project Structure

```
FunctionInliningPass/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── DESIGN.md               # Design decisions and alternatives
├── IMPLEMENTATION.md       # LLVM API details and pass internals
├── EVALUATION.md           # Metrics, test cases, baseline comparison
├── src/
│   └── InlinerPass.cpp     # Full pass implementation
├── tests/
│   ├── small.c             # Small inlinable function
│   ├── large.c             # Function too large to inline
│   ├── recursive.c         # Recursive function (should be skipped)
│   ├── mixed.c             # Mix of inlinable + non-inlinable
│   └── new_example.c       # Additional test case
├── scripts/
│   ├── build.sh            # Build the plugin
│   ├── generate_ir.sh      # Compile .c tests to LLVM IR
│   └── run.sh              # Run the pass on all IR files
├── ir/                     # Generated LLVM IR (input to pass)
└── optimized/              # Optimized IR output + pass logs
```

---

## Prerequisites

- `clang` (LLVM 14+)
- `opt`
- `llvm-config`
- `cmake` (3.16+)
- C++17 compiler (`g++` or `clang++`)

Install on Ubuntu/Debian:
```bash
sudo apt install clang llvm cmake build-essential
```

---

## How to Build

```bash
cd FunctionInliningPass
bash scripts/build.sh
```

Or manually:
```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
```

Expected output: `build/libFunctionInliningPass.so`

---

## How to Run

### Full pipeline (recommended):
```bash
bash scripts/build.sh
bash scripts/generate_ir.sh
bash scripts/run.sh 10
```

### Run on a single file:
```bash
opt -load-pass-plugin ./build/libFunctionInliningPass.so \
    -passes="simple-inline-pass" \
    -simple-inline-threshold=10 \
    ir/small.ll -S -o optimized/small.optimized.ll
```

### Check results:
```bash
cat optimized/small.log          # pass decision log
diff ir/small.ll optimized/small.optimized.ll  # IR diff
```

---

## Configurable Threshold

```bash
bash scripts/run.sh 15    # inline functions with cost < 15
bash scripts/run.sh 5     # stricter — inline only very small functions
```

Default threshold: `10`

---

## Example Output

For `tests/small.c` (a simple `add` function):
```
[simple-inline-pass] Function: add | Cost: 8 | INLINE
[simple-inline-pass] Inlined function: add
[simple-inline-pass] Removing dead function: add
[simple-inline-pass] ===== Pass Summary =====
[simple-inline-pass] Calls inspected: 1
[simple-inline-pass] Calls inlined: 1
[simple-inline-pass] Dead functions removed: 1
```

---

## Test Cases

| Test File     | Expected Behavior                                      |
|---------------|--------------------------------------------------------|
| `small.c`     | `add()` inlined and removed as dead                   |
| `large.c`     | Large helper skipped (cost exceeds threshold)          |
| `recursive.c` | `fact()` skipped due to direct recursion              |
| `mixed.c`     | Small functions inlined; recursive/large ones skipped |
| `new_example.c` | `inc()` inlined; `big_fn()` skipped by cost         |

See `EVALUATION.md` for detailed metrics and baseline comparison.
