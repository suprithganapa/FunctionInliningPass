# DESIGN.md — FunctionInliningPass

## Problem Statement

Function calls in compiled code introduce overhead: pushing arguments, saving registers, jumping to the callee, and returning. For small, frequently-called functions, this overhead can outweigh the actual work done. **Function inlining** eliminates the call by inserting the callee's body directly at the call site, enabling further optimizations like constant folding and register allocation across what were previously function boundaries.

The goal of this pass is to implement a **custom, cost-aware inlining pass** for LLVM IR that is transparent, configurable, and safe — avoiding correctness issues like infinite inlining of recursive functions.

---

## Design Goals

| Goal | Decision |
|------|----------|
| Correctness | Never inline recursive functions; check inlining success |
| Performance | Inline only small functions (cost < threshold) |
| Transparency | Log every decision to `stderr` with cost + reason |
| Configurability | Threshold exposed as a command-line option |
| Cleanliness | Remove dead functions and unreachable blocks after inlining |

---

## Approach

### Module-Level Pass

The pass operates at **module level** (not function level) because inlining decisions require seeing both the caller and callee at the same time, and dead function elimination requires iterating over all functions in the module.

### Two-Phase Design

**Phase 1 — Collect then Inline:**  
All call sites are collected into a `SmallVector` *before* any inlining begins. This is critical — iterating over instructions while modifying them causes iterator invalidation. Pre-collecting the call sites avoids undefined behavior.

**Phase 2 — Cleanup:**  
After inlining, the pass runs two cleanup steps:
1. `removeUnreachableBlocks()` on all callers that were modified (inlining can leave unreachable code paths)
2. Dead function elimination — iterates until no more dead functions are found (fixpoint loop), since removing one function can expose another as dead

---

## Cost Model

The cost model assigns weights to each instruction in the callee:

| Instruction Type | Weight |
|------------------|--------|
| Binary operators (add, mul, etc.) | 1 |
| Calls | 3 |
| Branches / switches | 2 |
| Everything else (load, store, return, etc.) | 1 |
| Back-edge bonus (likely loop) | +5 |

A back-edge is detected by checking if any basic block has a successor that was already visited in IR order — a lightweight heuristic for loop detection.

**Threshold:** Functions with `cost >= threshold` are NOT inlined. Default is `10`, configurable with `-simple-inline-threshold=N`.

---

## Inlining Rules

A call site is inlined only if ALL of the following hold:

1. The callee is a defined function (not just a declaration/external)
2. The callee is not named `main`
3. The callee is not directly recursive (calls itself)
4. The computed cost is strictly below the threshold

---

## Alternatives Considered

### Alternative 1: Always-Inline (No Cost Model)
Inline every non-recursive function regardless of size.

**Rejected because:** Large functions inlined into many callers bloat code size (code-size explosion). It can also hurt instruction cache performance by generating huge caller functions.

### Alternative 2: Call Count Heuristic
Inline a function only if it is called more than N times (favoring hot functions).

**Rejected because:** Requires profiling data or a use-count analysis pass. Adds complexity without a clear win for this project's scope.

### Alternative 3: Function-Level Pass with Inline Hints
Use a `FunctionPass` and mark eligible callees with `alwaysinline` attribute.

**Rejected because:** A function-level pass cannot see the whole module at once, making dead function elimination impossible. Module-level is strictly more powerful.

### Alternative 4: Use LLVM's Built-in Inliner (`-inline` pass)
Just use LLVM's existing inlining pass.

**Rejected because:** The goal of this project is to implement and understand the inlining mechanism at the LLVM API level — using the built-in pass provides no learning value and no custom control.

---

## Known Limitations

- **Indirect recursion:** If `f` calls `g` and `g` calls `f`, neither is flagged as recursive by the current guard. This could cause infinite inlining in theory, but is avoided in practice because after inlining `f` into `g`, the call to `f` would be from `main` or another function, not `g` itself.
- **No inlining of callees with `byval` parameters or varargs:** These are more complex cases not handled here.
- **Single-pass:** The pass runs once. A production inliner would iterate to a fixpoint.

---

## Future Extensions

- Indirect recursion detection via call graph SCC analysis
- Hot-path weighting using `BranchProbabilityInfo`
- Iterative inlining until fixpoint
- Integration with `InlineCost` LLVM API for a more accurate model
