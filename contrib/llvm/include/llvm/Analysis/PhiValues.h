//===- PhiValues.h - Phi Value Analysis -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the PhiValues class, and associated passes, which can be
// used to find the underlying values of the phis in a function, i.e. the
// non-phi values that can be found by traversing the phi graph.
//
// This information is computed lazily and cached. If new phis are added to the
// function they are handled correctly, but if an existing phi has its operands
// modified PhiValues has to be notified by calling invalidateValue.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_PHIVALUES_H
#define LLVM_ANALYSIS_PHIVALUES_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Pass.h"

namespace llvm {

class Use;
class Value;
class PHINode;
class Function;

/// Class for calculating and caching the underlying values of phis in a
/// function.
///
/// Initially the PhiValues is empty, and gets incrementally populated whenever
/// it is queried.
class PhiValues {
public:
  using ValueSet = SmallPtrSet<Value *, 4>;

  /// Construct an empty PhiValues.
  PhiValues(const Function &F) : F(F) {}

  /// Get the underlying values of a phi.
  ///
  /// This returns the cached value if PN has previously been processed,
  /// otherwise it processes it first.
  const ValueSet &getValuesForPhi(const PHINode *PN);

  /// Notify PhiValues that the cached information using V is no longer valid
  ///
  /// Whenever a phi has its operands modified the cached values for that phi
  /// (and the phis that use that phi) become invalid. A user of PhiValues has
  /// to notify it of this by calling invalidateValue on either the operand or
  /// the phi, which will then clear the relevant cached information.
  void invalidateValue(const Value *V);

  /// Free the memory used by this class.
  void releaseMemory();

  /// Print out the values currently in the cache.
  void print(raw_ostream &OS) const;

  /// Handle invalidation events in the new pass manager.
  bool invalidate(Function &, const PreservedAnalyses &,
                  FunctionAnalysisManager::Invalidator &);

private:
  using PhiSet = SmallPtrSet<const PHINode *, 4>;
  using ConstValueSet = SmallPtrSet<const Value *, 4>;

  /// The next depth number to be used by processPhi.
  unsigned int NextDepthNumber = 1;

  /// Depth numbers of phis. Phis with the same depth number are part of the
  /// same strongly connected component.
  DenseMap<const PHINode *, unsigned int> DepthMap;

  /// Non-phi values reachable from each component.
  DenseMap<unsigned int, ValueSet> NonPhiReachableMap;

  /// All values reachable from each component.
  DenseMap<unsigned int, ConstValueSet> ReachableMap;

  /// The function that the PhiValues is for.
  const Function &F;

  /// Process a phi so that its entries in the depth and reachable maps are
  /// fully populated.
  void processPhi(const PHINode *PN, SmallVector<const PHINode *, 8> &Stack);
};

/// The analysis pass which yields a PhiValues
///
/// The analysis does nothing by itself, and just returns an empty PhiValues
/// which will get filled in as it's used.
class PhiValuesAnalysis : public AnalysisInfoMixin<PhiValuesAnalysis> {
  friend AnalysisInfoMixin<PhiValuesAnalysis>;
  static AnalysisKey Key;

public:
  using Result = PhiValues;
  PhiValues run(Function &F, FunctionAnalysisManager &);
};

/// A pass for printing the PhiValues for a function.
///
/// This pass doesn't print whatever information the PhiValues happens to hold,
/// but instead first uses the PhiValues to analyze all the phis in the function
/// so the complete information is printed.
class PhiValuesPrinterPass : public PassInfoMixin<PhiValuesPrinterPass> {
  raw_ostream &OS;

public:
  explicit PhiValuesPrinterPass(raw_ostream &OS) : OS(OS) {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

/// Wrapper pass for the legacy pass manager
class PhiValuesWrapperPass : public FunctionPass {
  std::unique_ptr<PhiValues> Result;

public:
  static char ID;
  PhiValuesWrapperPass();

  PhiValues &getResult() { return *Result; }
  const PhiValues &getResult() const { return *Result; }

  bool runOnFunction(Function &F) override;
  void releaseMemory() override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

} // namespace llvm

#endif
