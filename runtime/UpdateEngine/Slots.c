#include <stdlib.h>

#include "UpdateEngine.h"

pthread_mutex_t Mutexes[MaxNumFilters];

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

int UnlinkOperation(struct Operation *Op, struct Operation **List) {
  if (*List == NULL)
    return -1;
  if (*List == Op) {
    *List = Op->Next;
    return 0;
  }
  return UnlinkOperation(Op, &(*List)->Next);
}

void EnterCriticalRegion(void *Arg) {
  unsigned FilterID = (unsigned)Arg;
  pthread_mutex_lock(&Mutexes[FilterID]);
}

void ExitCriticalRegion(void *Arg) {
  unsigned FilterID = (unsigned)Arg;
  pthread_mutex_unlock(&Mutexes[FilterID]);
}
