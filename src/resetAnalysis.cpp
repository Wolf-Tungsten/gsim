#include "common.h"
#include <map>
/* before splitted array  */

void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);

std::map<Node*, clockVal*> resetMap;

ResetType Node::inferReset() {
  if (reset != UNCERTAIN) return reset;
  Assert(assignTree.size() > 0, "empty assignTree");
  for (ExpTree* tree : assignTree) {
    ResetType newReset = tree->getRoot()->inferReset();
    if (reset == UNCERTAIN) reset = newReset;
    else if (reset != tree->getRoot()->reset) {
      printf("reset %d %d\n", reset, tree->getRoot()->reset);
      Panic();
    }
  }
  return reset;
}

ResetType ENode::inferReset() {
  if (reset != UNCERTAIN) return reset;
  if (nodePtr) {
    reset = nodePtr->inferReset(); 
    return reset;
  }
  auto propagateFromChildren = [this]() {
    ResetType inferred = UINTRESET;
    for (size_t i = 0; i < getChildNum(); i ++) {
      ENode* childNode = getChild(i);
      if (!childNode) continue;
      ResetType childReset = childNode->inferReset();
      if (childReset == ASYRESET) return ASYRESET;
    }
    return inferred;
  };
  int base;
  std::string str;
  switch (opType) {
    case OP_ASUINT:
    case OP_ASSINT:
      reset = propagateFromChildren();
      break;
    case OP_INT:
      std::tie(base, str) = firStrBase(strVal);
      if (str == "h0" || str == "0")
        reset = ZERO_RESET;
      else {
        std::cout << "Unknown :" << str << std::endl;
        TODO();
      }
      break;
    case OP_ASASYNCRESET:
      reset = ASYRESET;
      break;
    case OP_BITS:
    case OP_OR:
      reset = propagateFromChildren();
      break;
    case OP_NOT:
    case OP_AND:
    case OP_ANDR:
    case OP_ORR:
    case OP_XORR:
    case OP_XOR:
    case OP_CVT:
    case OP_NEG:
    case OP_PAD:
    case OP_HEAD:
    case OP_TAIL:
    case OP_MUX:
    case OP_WHEN:
    case OP_SHL:
    case OP_SHR:
    case OP_DSHL:
    case OP_DSHR:
    case OP_EQ:
    case OP_NEQ:
    case OP_LT:
    case OP_LEQ:
    case OP_GT:
    case OP_GEQ:
    case OP_CAT:
      reset = propagateFromChildren();
      break;
    default:
      reset = propagateFromChildren();
      break;
  }
  return reset;
}

void fillOuterWhen(ExpTree* newTree, ENode* enode) {
  ENode* whenNode = newTree->getRoot();
  while (whenNode->opType == OP_WHEN) {
    if (!whenNode->getChild(1)) whenNode->setChild(1, enode);
    if (!whenNode->getChild(2)) whenNode->setChild(2, enode);
    if (whenNode->getChild(1) && whenNode->getChild(2)) break;
    else if (whenNode->getChild(1)) whenNode = whenNode->getChild(1);
    else if (whenNode->getChild(2)) whenNode = whenNode->getChild(2);
    else Assert(0, "emptyWhen");
  }
}

void Node::addReset() { // remove
  Assert(type == NODE_REG_SRC, "%s(%d) is not regsrc", name.c_str(), type);

  ResetType resetType = resetCond->getRoot()->inferReset();
  reset = resetType;
  Assert(resetType != UNCERTAIN, "reset %s is uncertain", name.c_str());
}
