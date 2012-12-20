#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "loom/Utils.h"

int SendExactly(int Sock, const void *Buffer, size_t L) {
  size_t Sent = 0;
  while (Sent < L) {
    ssize_t R = send(Sock, (char *)Buffer + Sent, L - Sent, 0);
    if (R == -1) {
      perror("send");
      return -1;
    }
    Sent += R;
  }
  return 0;
}

int ReceiveExactly(int Sock, void *Buffer, size_t L) {
  size_t Received = 0;
  while (Received < L) {
    ssize_t R = recv(Sock, (char *)Buffer + Received, L - Received, 0);
    if (R == 0) {
      return -1;
    }
    if (R == -1) {
      perror("recv");
      return -1;
    }
    Received += R;
  }
  return 0;
}

int SendMessage(int Sock, const char *M) {
  uint32_t L = htonl(strlen(M));
  if (SendExactly(Sock, &L, sizeof(uint32_t)) == -1)
    return -1;
  if (SendExactly(Sock, M, strlen(M)) == -1)
    perror("send");
  return 0;
}

int ReceiveMessage(int Sock, char *M) {
  uint32_t L;
  if (ReceiveExactly(Sock, &L, sizeof(int)) == -1)
    return -1;
  L = ntohl(L);
  if (L >= MaxBufferSize) {
    fprintf(stderr, "message too long: length = %u\n", L);
    return -1;
  }
  if (ReceiveExactly(Sock, M, L) == -1)
    return -1;
  M[L] = '\0';
  return 0;
}
