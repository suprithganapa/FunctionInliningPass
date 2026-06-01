# IMPLEMENTATION — LLVM Details

## Source File

`src/InlinePass.cpp` — single translation unit, ~280 lines.

---

## LLVM Infrastructure Used

### Pass Registration (New Pass Manager)

```cpp
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "InlinePass", "0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM, ...) {
                  if (Name == "inline-pass") {
                    MPM.addPass(InlinePass());
                    return true;
                  }
                  return false;
                });
          }};
}
```

- `LLVM_ATTRIBUTE_WEAK` is required so the DSO symbol resolves correctly
  when `opt --load-pass-plugin` dlopen's the shared library.
- `LLVM_PLUGIN_API_VERSION` ensures ABI compatibility between the plugin and
  the host `opt` binary.
- Registered as a **module** pass (`ModulePassManager`) because inlining
  requires cross-function mutation.

### Pass Entry Point

```cpp
class InlinePass : public PassInfoMixin<InlinePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};
```

`PassInfoMixin` provides the boilerplate (`name()`, `isRequired()`, etc.)
so only `run` needs to be implemented.

### CallGraph Analysis

```cpp
CallGraph &CG = MAM.getResult<CallGraphAnalysis>(M);
```

`CallGraphAnalysis` is a built-in LLVM module analysis. Requesting it through
`MAM` ensures it is computed lazily and cached. The returned `CallGraph`
maps each `Function*` to a `CallGraphNode*`, which stores outgoing call edges
as `(Optional<WeakTrackingVH>, CallGraphNode*)` pairs.

**Edge iteration (LLVM 14 API):**

```cpp
for (auto &Edge : *Node) {
    if (Edge.first) {                        // Optional has a value
        if (Value *V = *Edge.first) {        // WeakTrackingVH still live
            if (auto *CB = dyn_cast<CallBase>(V)) {
                Out.push_back(CB);
            }
        }
    }
}
```

`WeakTrackingVH` is a weak value handle — it nullifies itself if the
referenced `Value` is deleted. The double check (`Edge.first` then `*Edge.first`)
is therefore necessary for correctness.

> **LLVM 18+ note**: In LLVM 18 the `Optional<WeakTrackingVH>` was replaced
> with `std::optional<WeakTrackingVH>`. The code compiles unchanged because
> LLVM provides `llvm::Optional` as an alias, but you may see deprecation
> warnings. Replace `Edge.first` checks with `Edge.first.has_value()` to
> silence them.

### Inlining

```cpp
InlineFunctionInfo IFI;
InlineResult Result = InlineFunction(*CB, IFI);
```

`InlineFunction` (from `llvm/Transforms/Utils/Cloning.h`) performs the
actual splice:

1. Clones the callee's basic blocks into the caller.
2. Replaces the `call` instruction with the entry block of the clone.
3. Wires the return value(s) back to the original call's users.
4. Updates the `CallGraph` incrementally via `IFI`.

`InlineFunctionInfo` can be given a `CallGraph*` to keep CG edges
consistent post-inline; we rely on it implicitly here (the `CallGraph` is
already borrowed from `MAM`).

### Unreachable Block Removal

```cpp
#include "llvm/Transforms/Utils/Local.h"
removeUnreachableBlocks(F);   // returns true if anything changed
```

After inlining, the original call instruction is replaced in-place. The
block containing the call (if it was the only instruction after a
terminator) may become unreachable. This single utility call handles the
cleanup via a reachability DFS from the entry block.

### Dead Function Elimination

```cpp
for (Function &F : M) {
    if (!F.isDeclaration() && F.getName() != "main"
        && !F.hasAddressTaken() && F.use_empty()) {
        ToErase.push_back(&F);
    }
}
for (Function *F : ToErase) F->eraseFromParent();
```

`use_empty()` checks whether any IR `Value` still holds a reference to
the function (direct calls, function pointers, metadata). Iterating to
convergence handles cascading dead-code (removing A may make B dead).
`hasAddressTaken()` guards against removing functions whose address is
stored (e.g., callback tables).

### Command-Line Option

```cpp
static cl::opt<unsigned> InlineThreshold(
    "inline-threshold-cost",
    cl::desc("Instruction cost threshold for inline-pass"),
    cl::init(20));
```

`cl::opt` integrates with LLVM's command-line parser. Users pass
`--inline-threshold-cost=<N>` to `opt`; the value is read at pass
execution time.

---

## Build System

`src/CMakeLists.txt`:

```cmake
find_package(LLVM REQUIRED CONFIG)
# ...
add_library(InlinePass MODULE InlinePass.cpp)
target_link_libraries(InlinePass PUBLIC LLVM)
```

Key points:
- `MODULE` (not `SHARED`) produces a `dlopen`-able plugin, not a linkable
  shared library. `opt --load-pass-plugin` expects a `MODULE`.
- `-fno-rtti` is mandatory: LLVM headers disable RTTI, and mixing RTTI
  and non-RTTI translation units causes linker errors with `dynamic_cast`.
- `find_package(LLVM CONFIG)` sets `LLVM_INCLUDE_DIRS`, `LLVM_LIBRARY_DIRS`,
  and `LLVM_DEFINITIONS` automatically from the installed LLVM cmake config.

---

## Data Structures

| Name | Type | Purpose |
|------|------|---------|
| `CallSites` | `SmallVector<CallBase*, 64>` | Collected call sites to evaluate |
| `TouchedFunctions` | `SmallVector<Function*, 16>` | Callers modified by inlining (for unreachable-block cleanup) |
| `InlineStats` | plain struct | Counters for the summary log |
| `InlineDecision` | `{bool CanInline, int Cost}` | Return value of `shouldInline()` |

`SmallVector` is chosen over `std::vector` because LLVM's allocator can
service small-capacity vectors without a heap allocation (inline storage of
64 / 16 elements respectively).

---

## Preserved Analyses

```cpp
return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
```

- `none()` — the pass modified the IR; all cached analyses are invalidated.
- `all()` — no-op run; downstream passes can reuse cached results.

In a production pass you would invalidate only the analyses actually
affected (e.g., `PA.abandon<CallGraphAnalysis>()`). For simplicity, and
because inlining invalidates almost everything anyway, `none()` is correct.
