#include "kernel/types.h"
#include "user.h"

void print_primes(int pipe_input[]) {

  int first_ip;
  if (0 == read(pipe_input[0], &first_ip, sizeof(int))) {
    close(pipe_input[0]);
    exit(0);
  }

  printf("prime %d\n", first_ip);

  // if no exit then create child process
  int pipe_sub[2];
  pipe(pipe_sub);

  if (fork() == 0) {
    close(pipe_sub[1]);
    close(pipe_input[0]);
    print_primes(pipe_sub);
  } else {
    close(pipe_sub[0]);

    int value = 0;
    while (1) {
      if (0 == read(pipe_input[0], &value, sizeof(int))) {
        close(pipe_input[0]);
        close(pipe_sub[1]);
        wait(0);
        exit(0);
      }

      // divide by first num, send if not divisible
      if ((value % first_ip) != 0) {
        // write it to pipe
        if (sizeof(int) != write(pipe_sub[1], &value, sizeof(int))) {
          printf("Fail to write\n");
          exit(0);
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  // create pipe
  int pipe_input[2];
  pipe(pipe_input);

  if (fork() == 0) {
    close(pipe_input[1]);
    print_primes(pipe_input);
  } else {
    close(pipe_input[0]);
    // feed input till 280
    for (int i = 2; i < 280; i++) {
      if (sizeof(int) != write(pipe_input[1], &i, sizeof(int))) {
        printf("Fail to write\n");
      }
    }
    close(pipe_input[1]);
  }

  wait(0);
  exit(0);
}