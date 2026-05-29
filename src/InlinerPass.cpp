#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

static cl::opt<unsigned> InlineThreshold(
    "simple-inline-threshold",
    cl::desc("Instruction cost threshold for simple-inline-pass"),
    cl::init(10));

namespace {

struct InlineStats {
  unsigned CallsSeen = 0;
  unsigned InlinedCalls = 0;
  unsigned RecursiveBlocked = 0;
  unsigned ExternalBlocked = 0;
  unsigned LargeBlocked = 0;
  unsigned MainBlocked = 0;
  unsigned DeadFunctionsRemoved = 0;
  unsigned FunctionsWithUnreachableRemoved = 0;
};

class CostModel {
public:
  int computeInlineCost(const Function &F) const {
    int Cost = 0;
    const bool HasBackEdge = hasLikelyBackEdge(F);

    for (const BasicBlock &BB : F) {
      for (const Instruction &I : BB) {
        Cost += instructionWeight(I);
      }
    }

    if (HasBackEdge) {
      Cost += 5;
    }

    return Cost;
  }

private:
  int instructionWeight(const Instruction &I) const {
    if (isa<BinaryOperator>(I)) {
      return 1;
    }

    if (isa<CallBase>(I)) {
      return 3;
    }

    if (isa<BranchInst>(I) || isa<SwitchInst>(I)) {
      return 2;
    }

    return 1;
  }

  bool hasLikelyBackEdge(const Function &F) const {
    DenseSet<const BasicBlock *> Seen;

    for (const BasicBlock &BB : F) {
      Seen.insert(&BB);

      for (const BasicBlock *Succ : successors(&BB)) {
        if (Seen.contains(Succ)) {
          return true;
        }
      }
    }

    return false;
  }
};

class SimpleInlinePass : public PassInfoMixin<SimpleInlinePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    InlineStats Stats;
    CostModel Model;
    bool Changed = false;
    SmallVector<CallBase *, 64> CallSites;
    SmallVector<Function *, 16> TouchedFunctions;

    collectCallSites(M, CallSites);

    for (CallBase *CB : CallSites) {
      if (!CB || !CB->getFunction()) {
        continue;
      }

      ++Stats.CallsSeen;

      Function *Caller = CB->getFunction();
      Function *Callee = CB->getCalledFunction();
      if (!Callee) {
        errs() << "[simple-inline-pass] Skipping indirect call in function: "
               << Caller->getName() << "\n";
        continue;
      }

      const InlineDecision Decision = shouldInline(*Callee, Model, Stats);

      errs() << "[simple-inline-pass] Function: " << Callee->getName()
             << " | Cost: " << Decision.Cost << " | "
             << (Decision.CanInline ? "INLINE" : "SKIP") << "\n";

      if (!Decision.CanInline) {
        continue;
      }

      InlineFunctionInfo IFI;
      InlineResult Result = InlineFunction(*CB, IFI);

      if (!Result.isSuccess()) {
        errs() << "[simple-inline-pass] Inline failed at callsite for "
               << Callee->getName() << ": " << Result.getFailureReason()
               << "\n";
        continue;
      }

      ++Stats.InlinedCalls;
      Changed = true;
      TouchedFunctions.push_back(Caller);
      errs() << "[simple-inline-pass] Inlined function: " << Callee->getName()
             << "\n";
    }

    Changed |= cleanupUnreachableBlocks(TouchedFunctions, Stats);
    Changed |= removeDeadFunctions(M, Stats);
    printSummary(Stats);

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

private:
  struct InlineDecision {
    bool CanInline = false;
    int Cost = 0;
  };

  static void collectCallSites(Module &M, SmallVectorImpl<CallBase *> &Out) {
    for (Function &F : M) {
      if (F.isDeclaration()) {
        continue;
      }

      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (auto *CB = dyn_cast<CallBase>(&I)) {
            Out.push_back(CB);
          }
        }
      }
    }
  }

  static bool isDirectRecursive(const Function &F) {
    for (const BasicBlock &BB : F) {
      for (const Instruction &I : BB) {
        const auto *CB = dyn_cast<CallBase>(&I);
        if (!CB) {
          continue;
        }
        if (CB->getCalledFunction() == &F) {
          return true;
        }
      }
    }
    return false;
  }

  static InlineDecision shouldInline(Function &Callee, const CostModel &Model,
                                     InlineStats &Stats) {
    InlineDecision Decision;
    Decision.Cost = Model.computeInlineCost(Callee);

    if (Callee.isDeclaration()) {
      ++Stats.ExternalBlocked;
      return Decision;
    }

    if (Callee.getName() == "main") {
      ++Stats.MainBlocked;
      return Decision;
    }

    if (isDirectRecursive(Callee)) {
      ++Stats.RecursiveBlocked;
      errs() << "[simple-inline-pass] Skipping recursive function: "
             << Callee.getName() << "\n";
      return Decision;
    }

    if (Decision.Cost >= static_cast<int>(InlineThreshold)) {
      ++Stats.LargeBlocked;
      return Decision;
    }

    Decision.CanInline = true;
    return Decision;
  }

  static bool cleanupUnreachableBlocks(SmallVectorImpl<Function *> &Functions,
                                       InlineStats &Stats) {
    bool Changed = false;
    DenseSet<Function *> Seen;

    for (Function *F : Functions) {
      if (!F || F->isDeclaration()) {
        continue;
      }
      if (!Seen.insert(F).second) {
        continue;
      }

      if (removeUnreachableBlocks(*F)) {
        ++Stats.FunctionsWithUnreachableRemoved;
        Changed = true;
        errs() << "[simple-inline-pass] Removed unreachable blocks in: "
               << F->getName() << "\n";
      }
    }

    return Changed;
  }

  static bool removeDeadFunctions(Module &M, InlineStats &Stats) {
    bool Changed = false;
    bool LocalChange = true;

    while (LocalChange) {
      LocalChange = false;
      SmallVector<Function *, 16> ToErase;

      for (Function &F : M) {
        if (F.isDeclaration()) {
          continue;
        }
        if (F.getName() == "main") {
          continue;
        }
        if (F.hasAddressTaken()) {
          continue;
        }
        if (F.use_empty()) {
          ToErase.push_back(&F);
        }
      }

      for (Function *F : ToErase) {
        errs() << "[simple-inline-pass] Removing dead function: " << F->getName()
               << "\n";
        F->eraseFromParent();
        ++Stats.DeadFunctionsRemoved;
        Changed = true;
        LocalChange = true;
      }
    }

    return Changed;
  }

  static void printSummary(const InlineStats &Stats) {
    errs() << "[simple-inline-pass] ===== Pass Summary =====\n";
    errs() << "[simple-inline-pass] Calls inspected: " << Stats.CallsSeen
           << "\n";
    errs() << "[simple-inline-pass] Calls inlined: " << Stats.InlinedCalls
           << "\n";
    errs() << "[simple-inline-pass] Recursive blocks: " << Stats.RecursiveBlocked
           << "\n";
    errs() << "[simple-inline-pass] External/declaration skips: "
           << Stats.ExternalBlocked << "\n";
    errs() << "[simple-inline-pass] Main skips: " << Stats.MainBlocked << "\n";
    errs() << "[simple-inline-pass] Cost-based skips: " << Stats.LargeBlocked
           << "\n";
    errs() << "[simple-inline-pass] Dead functions removed: "
           << Stats.DeadFunctionsRemoved << "\n";
    errs() << "[simple-inline-pass] Functions cleaned for unreachable blocks: "
           << Stats.FunctionsWithUnreachableRemoved << "\n";
  }
};

} // namespace

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
