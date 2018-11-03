#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //sleep
#include <signal.h>
#include <sys/types.h> //caddr_t
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_types.h>
#endif

// ATTACH
#if !defined(PTRACE_ATTACH) && (defined(PT_ATTACH))
// #ifdef __APPLE__
// #define PTRACE_ATTACH PT_ATTACHEXC
// #else
#define PTRACE_ATTACH PT_ATTACH
// #endif
#endif

// DETACH
#if !defined(PTRACE_DETACH) && defined(PT_DETACH)
#define PTRACE_DETACH PT_DETACH
#endif

// PEEKDATA
#if !defined(PTRACE_PEEKDATA) && defined(PT_READ_D)
#define PTRACE_PEEKDATA PT_READ_D
#endif

//デバッグ用
#define P printf("%s, %d\n", __FILE__, __LINE__); fflush (stdout);

int main(int argc, char *argv[])
{
  int status;

  if (argc < 2) {
    fprintf(stderr, "specify pid\n");
    exit(1);
  }

  pid_t pid = atoi(argv[1]);
  printf("attach to %d\n", pid);

  if (ptrace(PTRACE_ATTACH, pid, NULL, 0) < 0) {
    perror("failed to attach");
    exit(1);
  }

  while (1) {
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
      break;
    } else if (WIFSIGNALED(status)) {
      printf("terminated by signal %d\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      printf("stopped by signal %d\n", WSTOPSIG(status));
    }

    ptrace(PT_STEP, pid, NULL, 0);
  }

  return 0;
}
