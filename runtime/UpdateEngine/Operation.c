#include <stdlib.h>

#include "Operation.h"

void ClearOperations(struct Operation **Op) {
  if (*Op != NULL) {
    ClearOperations(&(*Op)->Next);
    free(*Op);
    *Op = NULL;
  }
}

void PrependOperation(struct Operation *Op, struct Operation **Pos) {
  Op->Next = *Pos;
  *Pos = Op;
}
