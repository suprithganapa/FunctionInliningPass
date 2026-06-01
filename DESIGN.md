# DESIGN — InlinePass Approach and Alternatives

## Problem Statement

Function inlining is one of the highest-leverage compiler optimisations:
eliminating call overhead, enabling downstream constant propagation, and
shrinking the call graph for later passes. LLVM ships a production inliner
(`-inline` / `InlinerPass`), but it is deeply entangled with the legacy pass
manager, profile data infrastructure, and attribute propagation machinery.

This project implements a **standalone, teachable inliner** as an out-of-tree
LLVM module pass. The goals are:

- Demonstrate end-to-end LLVM pass infrastructure usage (new pass manager,
  `PassPlugin`, `ModuleAnalysisManager`).
- Show how a simple cost model plus a call graph can make inlining decisions
  that are measurably better than a naïve "always inline small functions"
  policy.
- Be easy to read, build, and extend.

---

## High-Level Approach

```
Module
  └─► CallGraphAnalysis          (LLVM built-in)
        └─► collectCallSites()   (iterate CG edges → CallBase*)
              └─► for each CallBase:
                    shouldInline() → CostModel + guards
                      ├─ INLINE → InlineFunction() + track caller
                      └─ SKIP   → log reason
              └─► cleanupUnreachableBlocks()   (post-inline hygiene)
              └─► removeDeadFunctions()        (iterative DCE sweep)
```

### Why a Module Pass?

Inlining is inherently a *cross-function* transform — the callee's body is
moved into the caller. LLVM's `FunctionPass` cannot add or remove functions
from the module, and cannot examine the full call graph. A `ModulePass` (new
PM: `PassInfoMixin<T>` on `Module`) can do both.

---

## Cost Model

The cost model assigns an integer score to each candidate callee:

| Component | Formula | Rationale |
|-----------|---------|-----------|
| Instruction weight | Σ `instructionWeight(I)` over all BBs | Approximates code-size growth after inlining |
| Back-edge penalty | +5 if any BB has a back-edge successor | Loops make inline expansion expensive |
| Call-frequency discount | `-(numReferences × 2)` | A function called many times amortises fixed costs |

**`instructionWeight` table:**

| Instruction class | Weight |
|-------------------|--------|
| `BinaryOperator`  | 1 |
| `CallBase`        | 3 |
| `BranchInst` / `SwitchInst` | 2 |
| Everything else   | 1 |

The score is compared against `InlineThreshold` (default **20**). Scores
below the threshold are eligible for inlining.

### Why Not Use LLVM's `InlineCost` API?

LLVM's built-in cost model (`llvm/Analysis/InlineCost.h`) is accurate but
complex: it simulates simplifications, accounts for attribute-based bonuses,
and requires a `TargetTransformInfo` query. Using it here would obscure the
core mechanics. The hand-rolled model is intentionally simple and auditable.

---

## Inlining Guards

| Guard | Condition | Why |
|-------|-----------|-----|
| External declaration | `Callee->isDeclaration()` | No IR body to inline |
| Direct recursion | CG self-edge on callee | Would produce infinite expansion |
| `main` function | `getName() == "main"` | Inlining `main` makes no semantic sense |
| Cost threshold | `Cost >= InlineThreshold` | Prevents code-size explosion |

**Indirect recursion** (mutual recursion): Not explicitly detected as a cycle
in the current implementation. Because both functions in a mutual-recursion
pair will typically exceed the cost threshold, they are blocked by the size
guard in practice. See *Future Work* below.

---

## Dead-Code Elimination (DCE)

After inlining, callees that have been fully absorbed may have no remaining
callers. A two-phase cleanup runs:

1. **`cleanupUnreachableBlocks`** — After each inlined call site, the caller
   may contain unreachable BBs (e.g., the now-dead call instruction's
   continuation). `removeUnreachableBlocks()` (LLVM utility) handles this.

2. **`removeDeadFunctions`** — Iteratively erases any non-`main`,
   non-address-taken function whose `use_empty()` is true. Iterates until
   convergence to handle chains (if A was the only caller of B, removing A
   may make B dead too).

---

## Alternatives Considered

### 1. Always-inline on `noinline` attribute removal

Remove `noinline` from small functions and let LLVM's stock inliner run.

**Rejected**: This delegates all decision-making to LLVM's pass and defeats
the educational goal of building an explicit cost model.

### 2. Function-level pass with module-level state via global variables

Collect functions in a `FunctionPass` and post-process in a second pass.

**Rejected**: The LLVM new pass manager makes two-pass coordination
cumbersome, and function-level visibility is insufficient for call-graph
queries.

### 3. Inline everything under a flat instruction-count cap

Count instructions only, no call-frequency discount.

**Rejected**: A function called from 10 sites with a cost of 18 would
otherwise be skipped, even though it clearly amortises well. The
`numReferences`-based discount handles this cleanly (see test03).

### 4. Use `InlineAdvisor` / ML-guided inlining

LLVM 12+ supports an ML-based inlining advisor.

**Rejected for this project**: requires TensorFlow/ONNX infrastructure and
obscures the algorithmic choices. Noted as a future extension.

---

## Future Work

- **Indirect recursion cycle detection**: Walk the CG with DFS to detect SCCs
  (Strongly Connected Components) and block inlining within any SCC with ≥2
  nodes.
- **Inlining order**: Currently processes call sites in arbitrary CG traversal
  order. A bottom-up post-order traversal (leaves first) would expose more
  inlining opportunities in the same pass.
- **Attribute propagation**: After inlining, propagate `readnone`/`readonly`
  and `nounwind` attributes from callee to caller where safe.
- **Profile-guided threshold**: Read block-frequency data (`BlockFrequencyInfo`)
  to weight hot paths more aggressively.
