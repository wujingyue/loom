#include <cstdlib>

#include "Operation.h"

void ClearOperations(Operation *&Op) {
  if (Op != NULL) {
    ClearOperations(Op->Next);
    delete Op;
    Op = NULL;
  }
}

void PrependOperation(Operation *Op, Operation *&Pos) {
  Op->Next = Pos;
  Pos = Op;
}
