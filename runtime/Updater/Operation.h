#ifndef __LOOM_OPERATION_H
#define __LOOM_OPERATION_H

typedef void *ArgumentType;
typedef void (*CallBackType)(ArgumentType);

struct Operation {
  CallBackType CallBack;
  ArgumentType Arg;
  Operation *Next;
};

void ClearOperations(Operation *&Op);
void PrependOperation(Operation *Op, Operation *&Pos);

#endif
