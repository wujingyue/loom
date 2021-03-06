#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "UpdateEngine.h"

int LoomSwitches[MaxNumFuncs];
struct Operation *LoomOperations[MaxNumInsts];
pthread_mutex_t Mutexes[MaxNumFilters];

void LoomSlot(unsigned SlotID) {
  struct Operation *Op;
  assert(SlotID < MaxNumInsts);
  for (Op = LoomOperations[SlotID]; Op; Op = Op->Next) {
    Op->CallBack(Op->Arg);
  }
}

int LoomSwitch(int FuncID) {
  return LoomSwitches[FuncID];
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
