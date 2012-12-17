#ifndef __LOOM_OPERATION_H
#define __LOOM_OPERATION_H

typedef void *ArgumentType;
typedef void (*CallBackType)(ArgumentType);

struct Operation {
  CallBackType CallBack;
  ArgumentType Arg;
  struct Operation *Next;
};

void ClearOperations(struct Operation **Op);
void PrependOperation(struct Operation *Op, struct Operation **Pos);

#endif
