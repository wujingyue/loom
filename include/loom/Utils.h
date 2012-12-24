/* This file will be included in C and C++ files. */

#ifndef __LOOM_UTILS_H
#define __LOOM_UTILS_H

#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MaxBufferSize (1024)

int SendMessage(int Sock, const char *M);
int ReceiveMessage(int Sock, char *M);
int SendExactly(int Sock, const void *Buffer, size_t L);
int ReceiveExactly(int Sock, void *Buffer, size_t L);

#ifdef __cplusplus
}
#endif

#endif
