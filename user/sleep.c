#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    printf("Usage: sleep <value>\n");
    exit(0);
  }
  int value = atoi(argv[1]);
  sleep(value);
  exit(0);
}