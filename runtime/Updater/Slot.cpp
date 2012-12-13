#include "Updater.h"

Operation *LoomOperations[MaxNumInsts];

extern "C" void LoomSlot(unsigned SlotID) {
  for (Operation *Op = LoomOperations[SlotID]; Op; Op = Op->Next)
    Op->CallBack(Op->Arg);
}
