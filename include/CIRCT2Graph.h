/*
  Generate design graph (the intermediate representation of the input circuit) from AST
*/
#ifndef CIRCT2GRAPH_H
#define CIRCT2GRAPH_H
#include "common.h"
#include "circt/Dialect/SV/SVDialect.h"
#include "circt/Dialect/SV/SVOps.h"
#include "circt/Dialect/HW/HWDialect.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/Comb/CombOps.h"
#include "circt/Dialect/Seq/SeqDialect.h"
#include "circt/Dialect/Seq/SeqOps.h"
#include "circt/Dialect/Emit/EmitDialect.h"
#include "circt/Dialect/Emit/EmitOps.h"
#include "circt/Dialect/OM/OMDialect.h"
#include "circt/Dialect/OM/OMOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/DenseMap.h"
#include <map>
using namespace mlir;
using namespace circt;

class CIRCT2Graph {
 public:
  CIRCT2Graph(ModuleOp module) {
    module->walk([&](hw::HWModuleOp hwModuleOp) { topModule = hwModuleOp; });
  }
  graph* generateGraph();
 private:
  graph* g;
  hw::HWModuleOp topModule;
  llvm::DenseMap<mlir::Value, Node*> valueNodeMap;

  void processInputPort();
  void processOperations();
  void processConstantOp(hw::ConstantOp constantOp);
  
  // Comb dialect operation processors
  void processCombAddOp(comb::AddOp op);
  void processCombSubOp(comb::SubOp op);
  void processCombMulOp(comb::MulOp op);
  void processCombDivUOp(comb::DivUOp op);
  void processCombDivSOp(comb::DivSOp op);
  void processCombModUOp(comb::ModUOp op);
  void processCombModSOp(comb::ModSOp op);
  void processCombAndOp(comb::AndOp op);
  void processCombOrOp(comb::OrOp op);
  void processCombXorOp(comb::XorOp op);
  void processCombICmpOp(comb::ICmpOp op);
  void processCombShlOp(comb::ShlOp op);
  void processCombShrUOp(comb::ShrUOp op);
  void processCombShrSOp(comb::ShrSOp op);
  void processCombConcatOp(comb::ConcatOp op);
  void processCombExtractOp(comb::ExtractOp op);
  void processCombMuxOp(comb::MuxOp op);
  void processCombParityOp(comb::ParityOp op);
  void processCombReplicateOp(comb::ReplicateOp op);
  // Note: comb.ReverseOp, comb.TruthTableOp are not available in current CIRCT version
  
  // HW array operation processors
  void processHWArrayCreateOp(hw::ArrayCreateOp op);
  void processHWArrayGetOp(hw::ArrayGetOp op);
  void processHWArrayInjectOp(hw::ArrayInjectOp op);
  void processHWArraySliceOp(hw::ArraySliceOp op);
  void processHWArrayConcatOp(hw::ArrayConcatOp op);
  void processHWAggregateConstantOp(hw::AggregateConstantOp op);

  // Helper methods
  Node* createBinaryOpNode(mlir::Value result, mlir::Value lhs, mlir::Value rhs, OPType opType);
  Node* createUnaryOpNode(mlir::Value result, mlir::Value input, OPType opType);
  Node* createTernaryOpNode(mlir::Value result, mlir::Value cond, mlir::Value trueVal, mlir::Value falseVal, OPType opType);
  Node* createArrayOpNode(mlir::Value result, const std::vector<mlir::Value>& inputs, OPType opType);
  ENode* createNodeFromValue(mlir::Value value, const std::string& name);
  std::pair<int, bool> getResultType(mlir::Value value);
  OPType getICmpPredicate(int predicate);
  std::string opType2String(OPType opType);
};

#endif
