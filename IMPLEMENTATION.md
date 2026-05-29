# IMPLEMENTATION.md — FunctionInliningPass

## LLVM Pass Framework Used

This pass uses the **New Pass Manager (NPM)** introduced in LLVM 14+. It does NOT use the legacy `FunctionPass` / `ModulePass` API.

### Key framework classes:

| Class | Role |
|-------|------|
| `PassInfoMixin<SimpleInlinePass>` | Base class for NPM passes; provides boilerplate |
| `ModuleAnalysisManager` | Passed to `run()`; used to query analyses (unused here) |
| `PassPluginLibraryInfo` | Struct returned by `llvmGetPassPluginInfo()` to register the plugin |
| `PassBuilder` | Used inside the registration callback to attach the pass to the pipeline |

---

## Plugin Registration

```cpp
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "FunctionInliningPass", "0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "simple-inline-pass") {
            MPM.addPass(SimpleInlinePass());
            return true;
          }
          return false;
        });
    }};
}
```

- `LLVM_ATTRIBUTE_WEAK` ensures the symbol is weak-linked and discoverable at runtime.
- `registerPipelineParsingCallback` hooks into `opt`'s `-passes=` argument parser so `simple-inline-pass` is recognized.

---

## Command-Line Option

```cpp
static cl::opt<unsigned> InlineThreshold(
    "simple-inline-threshold",
    cl::desc("Instruction cost threshold for simple-inline-pass"),
    cl::init(10));
```

Uses `llvm/Support/CommandLine.h`. The option is global and read at pass runtime. Invoked as:
```bash
opt ... -simple-inline-threshold=15 ...
```

---

## Pass Entry Point

```cpp
PreservedAnalyses run(Module &M, ModuleAnalysisManager &)
```

The pass entry point. Returns:
- `PreservedAnalyses::none()` — if IR was changed (forces re-analysis)
- `PreservedAnalyses::all()` — if no changes were made

---

## LLVM APIs Used

### 1. Call Site Collection
```cpp
dyn_cast<CallBase>(&I)
```
`CallBase` is the common base class for `CallInst` and `InvokeInst`. Collecting all call bases before modification avoids iterator invalidation.

### 2. Inlining
```cpp
InlineFunctionInfo IFI;
InlineResult Result = InlineFunction(*CB, IFI);
```
- `InlineFunction()` from `llvm/Transforms/Utils/Cloning.h` performs the actual inlining
- `InlineFunctionInfo` carries info about the inlining (e.g., inlined static allocas)
- `InlineResult` reports success or failure (and the failure reason string)

### 3. Recursion Detection
```cpp
dyn_cast<CallBase>(&I)->getCalledFunction() == &F
```
Checks if any call inside function `F` calls `F` itself.

### 4. Unreachable Block Removal
```cpp
removeUnreachableBlocks(F)
```
From `llvm/Transforms/Utils/BasicBlockUtils.h`. Removes basic blocks that have no predecessors (unreachable after CFG changes from inlining).

### 5. Dead Function Elimination
```cpp
F.use_empty()          // no remaining uses (callers)
F.hasAddressTaken()    // skip if address is taken (could be called indirectly)
F.eraseFromParent()    // remove from module
```

### 6. Back-edge Detection (Loop Heuristic)
```cpp
for (const BasicBlock *Succ : successors(&BB)) {
    if (Seen.contains(Succ)) return true;
}
```
Uses `llvm/IR/CFG.h` for `successors()`. Checks if any block's successor was already seen in forward IR order — a conservative but cheap loop heuristic.

---

## Data Structures

| Structure | Type | Purpose |
|-----------|------|---------|
| `CallSites` | `SmallVector<CallBase*, 64>` | Pre-collected call sites to avoid invalidation |
| `TouchedFunctions` | `SmallVector<Function*, 16>` | Callers modified by inlining (for cleanup) |
| `Seen` | `DenseSet<Function*>` | Deduplication in cleanup pass |
| `InlineStats` | plain struct | Counters for the summary log |

`SmallVector` and `DenseSet` are LLVM's performance-optimized equivalents of `std::vector` and `std::unordered_set`, using inline storage to avoid heap allocation for small sizes.

---

## Build System

Uses `add_llvm_pass_plugin()` from LLVM's CMake module `AddLLVM`:

```cmake
find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_llvm_pass_plugin(FunctionInliningPass src/InlinerPass.cpp)
```

This macro sets the correct shared library flags, RPATH, and LLVM link dependencies automatically. Output: `libFunctionInliningPass.so`.

---

## How Inlining Works Internally

When `InlineFunction(*CB, IFI)` is called:

1. LLVM clones all basic blocks of the callee
2. Arguments of the callee are mapped to the actual parameters at the call site
3. The cloned blocks are inserted into the caller's function body, just after the call instruction
4. The return value (if any) replaces the call's result
5. The original `call` instruction is removed

The caller's CFG is updated to connect the new blocks. This may leave some blocks unreachable (e.g., if the callee had early returns), which is why `removeUnreachableBlocks()` is called afterward.
