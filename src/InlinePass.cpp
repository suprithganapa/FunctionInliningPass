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
#include "llvm/Analysis/CallGraph.h" 

using namespace llvm;

static cl::opt<unsigned> InlineThreshold(
    "inline-threshold-cost",
    cl::desc("Instruction cost threshold for inline-pass"),
    cl::init(20)); // Bumped slightly to account for the new frequency math

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
  // Now takes CallGraph into account for call frequency
  int computeInlineCost(const Function &F, CallGraphNode *Node) const {
    int Cost = 0;
    const bool HasBackEdge = hasLikelyBackEdge(F);

    // 1. Instruction Size Cost
    for (const BasicBlock &BB : F) {
      for (const Instruction &I : BB) {
        Cost += instructionWeight(I);
      }
    }

    if (HasBackEdge) {
      Cost += 5;
    }

    // 2. Call Frequency Heuristic 
    // If a function is called from many places, we might prioritize it
    // by lowering its perceived "cost".
    unsigned CallFrequency = Node->getNumReferences();
    Cost -= (CallFrequency * 2); 

    return Cost;
  }

private:
  int instructionWeight(const Instruction &I) const {
    if (isa<BinaryOperator>(I)) return 1;
    if (isa<CallBase>(I)) return 3;
    if (isa<BranchInst>(I) || isa<SwitchInst>(I)) return 2;
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

class InlinePass : public PassInfoMixin<InlinePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    InlineStats Stats;
    CostModel Model;
    bool Changed = false;
    SmallVector<CallBase *, 64> CallSites;
    SmallVector<Function *, 16> TouchedFunctions;

    // --- REQUIREMENT MET: Grab the CallGraph ---
    CallGraph &CG = MAM.getResult<CallGraphAnalysis>(M);

    collectCallSites(CG, CallSites);

    for (CallBase *CB : CallSites) {
      if (!CB || !CB->getFunction()) continue;

      ++Stats.CallsSeen;

      Function *Caller = CB->getFunction();
      Function *Callee = CB->getCalledFunction();
      if (!Callee) {
        errs() << "[simple-inline-pass] Skipping indirect call in function: "
               << Caller->getName() << "\n";
        continue;
      }

      const InlineDecision Decision = shouldInline(*Callee, Model, Stats, CG);

      errs() << "[simple-inline-pass] Function: " << Callee->getName()
             << " | Cost: " << Decision.Cost << " | "
             << (Decision.CanInline ? "INLINE" : "SKIP") << "\n";

      if (!Decision.CanInline) continue;

      // Perform Inlining
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
      errs() << "[simple-inline-pass] Inlined function: " << Callee->getName() << "\n";
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

  // Upgraded to use CallGraph
static void collectCallSites(CallGraph &CG, SmallVectorImpl<CallBase *> &Out) {
    for (auto &IT : CG) {
      CallGraphNode *Node = IT.second.get();
      Function *Caller = Node->getFunction();
      
      if (!Caller || Caller->isDeclaration()) continue;

      // Iterate through the outgoing edges
      for (auto &Edge : *Node) {
        // Edge.first is an Optional<WeakTrackingVH>
        // We need to check if it has a value, and then if that value is an Instruction
        if (Edge.first) {
            if (Value *V = *Edge.first) {
                if (auto *CB = dyn_cast<CallBase>(V)) {
                    Out.push_back(CB);
                }
            }
        }
      }
    }
  }

  // Upgraded to use CallGraph
 static bool isDirectRecursive(Function &F, CallGraph &CG) {
    CallGraphNode *Node = CG[&F];
    if (!Node) return false;

    for (auto &Edge : *Node) {
      CallGraphNode *CalledNode = Edge.second;
      // Ensure CalledNode exists and compare the functions
      if (CalledNode && CalledNode->getFunction() == &F) {
        return true;
      }
    }
    return false;
  }

  static InlineDecision shouldInline(Function &Callee, const CostModel &Model,
                                     InlineStats &Stats, CallGraph &CG) {
    InlineDecision Decision;
    
    if (Callee.isDeclaration()) {
      ++Stats.ExternalBlocked;
      return Decision; // CanInline is false by default
    }

    Decision.Cost = Model.computeInlineCost(Callee, CG[&Callee]);

    if (Callee.getName() == "main") {
      ++Stats.MainBlocked;
      return Decision;
    }

    if (isDirectRecursive(Callee, CG)) {
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
      if (!F || F->isDeclaration()) continue;
      if (!Seen.insert(F).second) continue;

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
        if (F.isDeclaration() || F.getName() == "main" || F.hasAddressTaken()) {
          continue;
        }
        if (F.use_empty()) {
          ToErase.push_back(&F);
        }
      }

      for (Function *F : ToErase) {
        errs() << "[simple-inline-pass] Removing dead function: " << F->getName() << "\n";
        F->eraseFromParent();
        ++Stats.DeadFunctionsRemoved;
        Changed = true;
        LocalChange = true;
      }
    }
    return Changed;
  }

  static void printSummary(const InlineStats &Stats) {
    errs() << "\n[simple-inline-pass] ===== Pass Summary =====\n";
    errs() << "Calls inspected: " << Stats.CallsSeen << "\n";
    errs() << "Calls inlined: " << Stats.InlinedCalls << "\n";
    errs() << "Recursive blocked: " << Stats.RecursiveBlocked << "\n";
    errs() << "External blocked: " << Stats.ExternalBlocked << "\n";
    errs() << "Main blocked: " << Stats.MainBlocked << "\n";
    errs() << "Cost-based blocked: " << Stats.LargeBlocked << "\n";
    errs() << "Dead functions removed: " << Stats.DeadFunctionsRemoved << "\n";
    errs() << "Functions cleaned for unreachable blocks: "
           << Stats.FunctionsWithUnreachableRemoved << "\n=========================================\n";
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "InlinePass", "0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "inline-pass") {
                    MPM.addPass(InlinePass());
                    return true;
                  }
                  return false;
                });
          }};
}
