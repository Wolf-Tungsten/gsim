/*
  Generate design graph (the intermediate representation of the input circuit) from AST
*/
#include "CIRCT2Graph.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/Casting.h"
#include "mlir/IR/Block.h"

graph* CIRCT2Graph::generateGraph() {
  g = new graph();
  processInputPort();
  processOperations();

  // Debug output: show what we processed
  std::cout << "CIRCT2Graph processing completed:" << std::endl;
  std::cout << "  - Input ports: " << g->input.size() << std::endl;
  std::cout << "  - Total nodes: " << g->allNodes.size() << std::endl;
  std::cout << "  - Value mappings: " << valueNodeMap.size() << std::endl;

  return g;
}

void CIRCT2Graph::processInputPort() {
  size_t inputPortIdx = 0;
  for(size_t i = 0; i < topModule.getNumPorts(); i++) {
    auto portInfo = topModule.getPort(i);
    if(portInfo.isOutput()) continue;
    // 创建 typeInfo
    TypeInfo* typeInfo = new TypeInfo();
    typeInfo->set_sign(portInfo.type.isSignedInteger());
    typeInfo->set_width(portInfo.type.getIntOrFloatBitWidth());
    typeInfo->set_reset(UNCERTAIN);
    // 创建 node
    Node* node = new Node(NODE_INP); 
    node->name = topModule.getPortName(i).str();
    node->updateInfo(typeInfo);
    // 将 node 添加到图中
    g->input.push_back(node);
    // 将 value-node 关系添加到 map 中
    mlir::Value portValue = topModule.getBodyRegion().front().getArgument(inputPortIdx);
    valueNodeMap[portValue] = node;
    inputPortIdx++;
  }
}

void CIRCT2Graph::processOperations() {
  if (!topModule) return;
  Block* body = topModule.getBodyBlock();
  if (!body) return;

  for (auto& op : body->getOperations()) {
    // GRH: add more operation in this branch
    if (auto constantOp = llvm::dyn_cast<hw::ConstantOp>(op)) {
      processConstantOp(constantOp);
    } else if (auto addOp = llvm::dyn_cast<comb::AddOp>(op)) {
      processCombAddOp(addOp);
    } else if (auto subOp = llvm::dyn_cast<comb::SubOp>(op)) {
      processCombSubOp(subOp);
    } else if (auto mulOp = llvm::dyn_cast<comb::MulOp>(op)) {
      processCombMulOp(mulOp);
    } else if (auto divUOp = llvm::dyn_cast<comb::DivUOp>(op)) {
      processCombDivUOp(divUOp);
    } else if (auto divSOp = llvm::dyn_cast<comb::DivSOp>(op)) {
      processCombDivSOp(divSOp);
    } else if (auto modUOp = llvm::dyn_cast<comb::ModUOp>(op)) {
      processCombModUOp(modUOp);
    } else if (auto modSOp = llvm::dyn_cast<comb::ModSOp>(op)) {
      processCombModSOp(modSOp);
    } else if (auto andOp = llvm::dyn_cast<comb::AndOp>(op)) {
      processCombAndOp(andOp);
    } else if (auto orOp = llvm::dyn_cast<comb::OrOp>(op)) {
      processCombOrOp(orOp);
    } else if (auto xorOp = llvm::dyn_cast<comb::XorOp>(op)) {
      processCombXorOp(xorOp);
    } else if (auto icmpOp = llvm::dyn_cast<comb::ICmpOp>(op)) {
      processCombICmpOp(icmpOp);
    } else if (auto shlOp = llvm::dyn_cast<comb::ShlOp>(op)) {
      processCombShlOp(shlOp);
    } else if (auto shrUOp = llvm::dyn_cast<comb::ShrUOp>(op)) {
      processCombShrUOp(shrUOp);
    } else if (auto shrSOp = llvm::dyn_cast<comb::ShrSOp>(op)) {
      processCombShrSOp(shrSOp);
    } else if (auto concatOp = llvm::dyn_cast<comb::ConcatOp>(op)) {
      processCombConcatOp(concatOp);
    } else if (auto extractOp = llvm::dyn_cast<comb::ExtractOp>(op)) {
      processCombExtractOp(extractOp);
    } else if (auto muxOp = llvm::dyn_cast<comb::MuxOp>(op)) {
      processCombMuxOp(muxOp);
    } else if (auto parityOp = llvm::dyn_cast<comb::ParityOp>(op)) {
      processCombParityOp(parityOp);
    } else if (auto replicateOp = llvm::dyn_cast<comb::ReplicateOp>(op)) {
      processCombReplicateOp(replicateOp);
    } else if (auto arrayCreateOp = llvm::dyn_cast<hw::ArrayCreateOp>(op)) {
      processHWArrayCreateOp(arrayCreateOp);
    } else if (auto arrayGetOp = llvm::dyn_cast<hw::ArrayGetOp>(op)) {
      processHWArrayGetOp(arrayGetOp);
    } else if (auto arrayInjectOp = llvm::dyn_cast<hw::ArrayInjectOp>(op)) {
      processHWArrayInjectOp(arrayInjectOp);
    } else if (auto arraySliceOp = llvm::dyn_cast<hw::ArraySliceOp>(op)) {
      processHWArraySliceOp(arraySliceOp);
    } else if (auto arrayConcatOp = llvm::dyn_cast<hw::ArrayConcatOp>(op)) {
      processHWArrayConcatOp(arrayConcatOp);
    } else if (auto aggregateConstantOp = llvm::dyn_cast<hw::AggregateConstantOp>(op)) {
      processHWAggregateConstantOp(aggregateConstantOp);
    } else if (auto outputOp = llvm::dyn_cast<hw::OutputOp>(op)) {
      // hw.output 操作不创建新的节点，只处理其操作数
      for (auto operand : outputOp.getOperands()) {
        // 确保操作数已经被处理
        valueNodeMap[operand];
      }
    } else {
      Assert(false, "Unsupported operation: %s", op.getName().getStringRef().str().c_str());
    }
  }
}

void CIRCT2Graph::processConstantOp(hw::ConstantOp constantOp) {
  auto intType = llvm::dyn_cast<IntegerType>(constantOp.getType());
  Assert(intType, "hw.constant expects integer result type");

  bool isSigned = intType.isSignedInteger();
  int width = intType.getWidth();

  Node* node = new Node(NODE_OTHERS);
  node->name = format("const_%d", node->id);
  node->setType(width, isSigned);
  node->status = CONSTANT_NODE;

  const llvm::APInt& value = constantOp.getValue();
  llvm::SmallVector<char> lc;
  value.toString(lc, 10, isSigned);
  std::string literal(lc.begin(), lc.end());
  if (literal.empty()) literal = "0";

  ENode* intENode = new ENode(OP_INT);
  intENode->strVal = literal;
  intENode->width = width;
  intENode->sign = isSigned;

  ExpTree* expTree = new ExpTree(intENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[constantOp.getResult()] = node;
}

// Helper methods
Node* CIRCT2Graph::createBinaryOpNode(mlir::Value result, mlir::Value lhs, mlir::Value rhs, OPType opType) {
  Node* node = new Node(NODE_OTHERS);
  node->name = format("op_%s_%d", opType2String(opType).c_str(), node->id);
  auto [width, sign] = getResultType(result);
  node->setType(width, sign);

  ENode* opENode = new ENode(opType);
  opENode->addChild(createNodeFromValue(lhs, "lhs"));
  opENode->addChild(createNodeFromValue(rhs, "rhs"));
  opENode->setWidth(node->width, node->sign);

  ExpTree* expTree = new ExpTree(opENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[result] = node;
  return node;
}

Node* CIRCT2Graph::createUnaryOpNode(mlir::Value result, mlir::Value input, OPType opType) {
  Node* node = new Node(NODE_OTHERS);
  node->name = format("op_%s_%d", opType2String(opType).c_str(), node->id);
  auto [width, sign] = getResultType(result);
  node->setType(width, sign);

  ENode* opENode = new ENode(opType);
  opENode->addChild(createNodeFromValue(input, "input"));
  opENode->setWidth(node->width, node->sign);

  ExpTree* expTree = new ExpTree(opENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[result] = node;
  return node;
}

Node* CIRCT2Graph::createTernaryOpNode(mlir::Value result, mlir::Value cond, mlir::Value trueVal, mlir::Value falseVal, OPType opType) {
  Node* node = new Node(NODE_OTHERS);
  node->name = format("op_%s_%d", opType2String(opType).c_str(), node->id);
  auto [width, sign] = getResultType(result);
  node->setType(width, sign);

  ENode* opENode = new ENode(opType);
  opENode->addChild(createNodeFromValue(cond, "cond"));
  opENode->addChild(createNodeFromValue(trueVal, "trueVal"));
  opENode->addChild(createNodeFromValue(falseVal, "falseVal"));
  opENode->setWidth(node->width, node->sign);

  ExpTree* expTree = new ExpTree(opENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[result] = node;
  return node;
}

Node* CIRCT2Graph::createArrayOpNode(mlir::Value result, const std::vector<mlir::Value>& inputs, OPType opType) {
  Node* node = new Node(NODE_OTHERS);
  node->name = format("op_%s_%d", opType2String(opType).c_str(), node->id);
  auto [width, sign] = getResultType(result);
  node->setType(width, sign);

  ENode* opENode = new ENode(opType);
  for (size_t i = 0; i < inputs.size(); ++i) {
    opENode->addChild(createNodeFromValue(inputs[i], format("input_%zu", i)));
  }
  opENode->setWidth(node->width, node->sign);

  ExpTree* expTree = new ExpTree(opENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[result] = node;
  return node;
}

ENode* CIRCT2Graph::createNodeFromValue(mlir::Value value, const std::string& name) {
  auto it = valueNodeMap.find(value);
  if (it != valueNodeMap.end()) {
    return new ENode(it->second);
  }

  // If not found, this might be an intermediate value that needs to be processed
  // For now, return nullptr - this should be handled by the calling context
  return nullptr;
}

OPType CIRCT2Graph::getICmpPredicate(int predicate) {
  switch (predicate) {
    case 0: return OP_EQ;   // eq
    case 1: return OP_NEQ;  // ne
    case 2: return OP_LT;   // slt
    case 3: return OP_LEQ;  // sle
    case 4: return OP_GT;   // sgt
    case 5: return OP_GEQ;  // sge
    case 6: return OP_LT;   // ult (unsigned less than)
    case 7: return OP_LEQ;  // ule (unsigned less equal)
    case 8: return OP_GT;   // ugt (unsigned greater than)
    case 9: return OP_GEQ;  // uge (unsigned greater equal)
    default: Assert(false, "Unsupported ICMP predicate"); return OP_EQ;
  }
}

std::string CIRCT2Graph::opType2String(OPType opType) {
  switch (opType) {
    case OP_ADD: return "add";
    case OP_SUB: return "sub";
    case OP_MUL: return "mul";
    case OP_DIV: return "div";
    case OP_REM: return "rem";
    case OP_AND: return "and";
    case OP_OR: return "or";
    case OP_XOR: return "xor";
    case OP_SHL: return "shl";
    case OP_SHR: return "shr";
    case OP_EQ: return "eq";
    case OP_NEQ: return "neq";
    case OP_LT: return "lt";
    case OP_LEQ: return "leq";
    case OP_GT: return "gt";
    case OP_GEQ: return "geq";
    case OP_CAT: return "cat";
    case OP_BITS: return "bits";
    case OP_REPLICATE: return "replicate";
    case OP_MUX: return "mux";
    case OP_XORR: return "xorr";
    case OP_GROUP: return "group";
    default: return "unknown";
  }
}

std::pair<int, bool> CIRCT2Graph::getResultType(mlir::Value value) {
  auto type = value.getType();
  if (auto intType = llvm::dyn_cast<IntegerType>(type)) {
    return {intType.getWidth(), intType.isSignedInteger()};
  }
  if (auto arrayType = llvm::dyn_cast<hw::ArrayType>(type)) {
    // For arrays, we calculate total bit width as element_width * num_elements
    auto elementType = arrayType.getElementType();
    if (auto intElementType = llvm::dyn_cast<IntegerType>(elementType)) {
      int elementWidth = intElementType.getWidth();
      int numElements = arrayType.getNumElements();
      return {elementWidth * numElements, false}; // Arrays are typically unsigned
    }
    // For nested arrays or other element types, return a default width
    return {32, false};
  }
  // For other types, return default values
  return {32, true};
}

// Comb dialect operation processors
void CIRCT2Graph::processCombAddOp(comb::AddOp addOp) {
  std::cout << "Processing comb.add operation" << std::endl;
  createBinaryOpNode(addOp.getResult(), addOp.getOperand(0), addOp.getOperand(1), OP_ADD);
}

void CIRCT2Graph::processCombSubOp(comb::SubOp subOp) {
  createBinaryOpNode(subOp.getResult(), subOp.getOperand(0), subOp.getOperand(1), OP_SUB);
}

void CIRCT2Graph::processCombMulOp(comb::MulOp mulOp) {
  createBinaryOpNode(mulOp.getResult(), mulOp.getOperand(0), mulOp.getOperand(1), OP_MUL);
}

void CIRCT2Graph::processCombDivUOp(comb::DivUOp divUOp) {
  createBinaryOpNode(divUOp.getResult(), divUOp.getOperand(0), divUOp.getOperand(1), OP_DIV);
}

void CIRCT2Graph::processCombDivSOp(comb::DivSOp divSOp) {
  createBinaryOpNode(divSOp.getResult(), divSOp.getOperand(0), divSOp.getOperand(1), OP_DIV);
}

void CIRCT2Graph::processCombModUOp(comb::ModUOp modUOp) {
  createBinaryOpNode(modUOp.getResult(), modUOp.getOperand(0), modUOp.getOperand(1), OP_REM);
}

void CIRCT2Graph::processCombModSOp(comb::ModSOp modSOp) {
  createBinaryOpNode(modSOp.getResult(), modSOp.getOperand(0), modSOp.getOperand(1), OP_REM);
}

void CIRCT2Graph::processCombAndOp(comb::AndOp andOp) {
  createBinaryOpNode(andOp.getResult(), andOp.getOperand(0), andOp.getOperand(1), OP_AND);
}

void CIRCT2Graph::processCombOrOp(comb::OrOp orOp) {
  createBinaryOpNode(orOp.getResult(), orOp.getOperand(0), orOp.getOperand(1), OP_OR);
}

void CIRCT2Graph::processCombXorOp(comb::XorOp xorOp) {
  createBinaryOpNode(xorOp.getResult(), xorOp.getOperand(0), xorOp.getOperand(1), OP_XOR);
}

void CIRCT2Graph::processCombICmpOp(comb::ICmpOp icmpOp) {
  OPType predicate = getICmpPredicate(static_cast<int>(icmpOp.getPredicate()));
  std::cout << "Processing comb.icmp operation with predicate " << predicate << std::endl;
  createBinaryOpNode(icmpOp.getResult(), icmpOp.getOperand(0), icmpOp.getOperand(1), predicate);
}

void CIRCT2Graph::processCombShlOp(comb::ShlOp shlOp) {
  createBinaryOpNode(shlOp.getResult(), shlOp.getOperand(0), shlOp.getOperand(1), OP_SHL);
}

void CIRCT2Graph::processCombShrUOp(comb::ShrUOp shrUOp) {
  createBinaryOpNode(shrUOp.getResult(), shrUOp.getOperand(0), shrUOp.getOperand(1), OP_SHR);
}

void CIRCT2Graph::processCombShrSOp(comb::ShrSOp shrSOp) {
  createBinaryOpNode(shrSOp.getResult(), shrSOp.getOperand(0), shrSOp.getOperand(1), OP_SHR);
}

void CIRCT2Graph::processCombConcatOp(comb::ConcatOp concatOp) {
  std::cout << "Processing comb.concat operation with " << concatOp.getOperands().size() << " operands" << std::endl;
  // Concat operation needs special handling since it can have multiple operands
  Node* node = new Node(NODE_OTHERS);
  node->name = format("concat_%d", node->id);
  auto [width, sign] = getResultType(concatOp.getResult());
  node->setType(width, sign);

  ENode* concatENode = new ENode(OP_CAT);
  for (auto operand : concatOp.getOperands()) {
    concatENode->addChild(createNodeFromValue(operand, "operand"));
  }
  concatENode->setWidth(node->width, node->sign);

  ExpTree* expTree = new ExpTree(concatENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[concatOp.getResult()] = node;
}

void CIRCT2Graph::processCombExtractOp(comb::ExtractOp extractOp) {
  Node* node = new Node(NODE_OTHERS);
  node->name = format("extract_%d", node->id);
  auto [width, sign] = getResultType(extractOp.getResult());
  node->setType(width, sign);

  ENode* extractENode = new ENode(OP_BITS);
  extractENode->addChild(createNodeFromValue(extractOp.getInput(), "input"));

  // Add bit range information as parameters
  uint32_t lowBit = extractOp.getLowBit();
  extractENode->values.push_back(lowBit);

  ExpTree* expTree = new ExpTree(extractENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[extractOp.getResult()] = node;
}

void CIRCT2Graph::processCombMuxOp(comb::MuxOp muxOp) {
  std::cout << "Processing comb.mux operation" << std::endl;
  createTernaryOpNode(muxOp.getResult(), muxOp.getCond(), muxOp.getTrueValue(), muxOp.getFalseValue(), OP_MUX);
}

void CIRCT2Graph::processCombParityOp(comb::ParityOp parityOp) {
  createUnaryOpNode(parityOp.getResult(), parityOp.getInput(), OP_XORR);
}

void CIRCT2Graph::processCombReplicateOp(comb::ReplicateOp replicateOp) {
  std::cout << "Processing comb.replicate operation" << std::endl;
  Node* node = new Node(NODE_OTHERS);
  node->name = format("replicate_%d", node->id);
  auto [width, sign] = getResultType(replicateOp.getResult());
  node->setType(width, sign);

  ENode* replicateENode = new ENode(OP_REPLICATE);
  replicateENode->addChild(createNodeFromValue(replicateOp.getInput(), "input"));

  // Add replicate count as parameter
  uint32_t count = replicateOp.getMultiple();
  replicateENode->values.push_back(count);
  replicateENode->setWidth(node->width, node->sign);

  ExpTree* expTree = new ExpTree(replicateENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[replicateOp.getResult()] = node;
}

// HW array operation processors
void CIRCT2Graph::processHWArrayCreateOp(hw::ArrayCreateOp arrayCreateOp) {
  std::cout << "Processing hw.array_create operation with " << arrayCreateOp.getInputs().size() << " inputs" << std::endl;
  std::vector<mlir::Value> inputs(arrayCreateOp.getInputs().begin(), arrayCreateOp.getInputs().end());
  createArrayOpNode(arrayCreateOp.getResult(), inputs, OP_GROUP);
}

void CIRCT2Graph::processHWArrayGetOp(hw::ArrayGetOp arrayGetOp) {
  std::cout << "Processing hw.array_get operation" << std::endl;
  Node* node = new Node(NODE_OTHERS);
  node->name = format("array_get_%d", node->id);
  auto [width, sign] = getResultType(arrayGetOp.getResult());
  node->setType(width, sign);

  ENode* getENode = new ENode(OP_INDEX);
  getENode->addChild(createNodeFromValue(arrayGetOp.getInput(), "array"));
  getENode->addChild(createNodeFromValue(arrayGetOp.getIndex(), "index"));
  getENode->setWidth(node->width, node->sign);

  ExpTree* expTree = new ExpTree(getENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[arrayGetOp.getResult()] = node;
}

void CIRCT2Graph::processHWArrayInjectOp(hw::ArrayInjectOp arrayInjectOp) {
  std::cout << "Processing hw.array_inject operation" << std::endl;
  std::vector<mlir::Value> inputs = {arrayInjectOp.getInput(), arrayInjectOp.getIndex(), arrayInjectOp.getElement()};
  createArrayOpNode(arrayInjectOp.getResult(), inputs, OP_GROUP);
}

void CIRCT2Graph::processHWArraySliceOp(hw::ArraySliceOp arraySliceOp) {
  std::cout << "Processing hw.array_slice operation" << std::endl;
  Node* node = new Node(NODE_OTHERS);
  node->name = format("array_slice_%d", node->id);
  auto [width, sign] = getResultType(arraySliceOp.getDst());
  node->setType(width, sign);

  ENode* sliceENode = new ENode(OP_BITS);
  sliceENode->addChild(createNodeFromValue(arraySliceOp.getInput(), "array"));
  sliceENode->addChild(createNodeFromValue(arraySliceOp.getLowIndex(), "lowIndex"));
  sliceENode->setWidth(node->width, node->sign);

  ExpTree* expTree = new ExpTree(sliceENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[arraySliceOp.getDst()] = node;
}

void CIRCT2Graph::processHWArrayConcatOp(hw::ArrayConcatOp arrayConcatOp) {
  std::cout << "Processing hw.array_concat operation with " << arrayConcatOp.getInputs().size() << " inputs" << std::endl;
  std::vector<mlir::Value> inputs(arrayConcatOp.getInputs().begin(), arrayConcatOp.getInputs().end());
  createArrayOpNode(arrayConcatOp.getResult(), inputs, OP_CAT);
}

void CIRCT2Graph::processHWAggregateConstantOp(hw::AggregateConstantOp aggregateConstantOp) {
  std::cout << "Processing hw.aggregate_constant operation" << std::endl;
  Node* node = new Node(NODE_OTHERS);
  node->name = format("aggregate_const_%d", node->id);
  auto [width, sign] = getResultType(aggregateConstantOp.getResult());
  node->setType(width, sign);
  node->status = CONSTANT_NODE;

  // For aggregate constants, we create a simple group operation
  ENode* constENode = new ENode(OP_GROUP);
  constENode->setWidth(node->width, node->sign);

  ExpTree* expTree = new ExpTree(constENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[aggregateConstantOp.getResult()] = node;
}
