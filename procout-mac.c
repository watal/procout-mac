#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //sleep
#include <sys/types.h> //caddr_t
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>

#if !defined(PTRACE_ATTACH) && (defined(PT_ATTACHE) || defined(PT_ATTACHEXC))
#define PTRACE_ATTACH PT_ATTACHEXC
#endif

#if !defined(PTRACE_DETACH) && defined(PT_DETACH)
#define PTRACE_DETACH PT_DETACH
#endif

#if !defined(PTRACE_PEEKDATA) && defined(PT_READ_D)
#define PTRACE_PEEKDATA PT_READ_D
#endif

//#if !defined(PTRACE_SYSCALL) && defined(PT_CONTINUE)
//#define PTRACE_SYSCALL PT_CONTINUE
//#endif
//

#ifndef __linux__
struct user_regs_struct
{
  __extension__ unsigned long long int r15;
  __extension__ unsigned long long int r14;
  __extension__ unsigned long long int r13;
  __extension__ unsigned long long int r12;
  __extension__ unsigned long long int rbp;
  __extension__ unsigned long long int rbx;
  __extension__ unsigned long long int r11;
  __extension__ unsigned long long int r10;
  __extension__ unsigned long long int r9;
  __extension__ unsigned long long int r8;
  __extension__ unsigned long long int rax;
  __extension__ unsigned long long int rcx;
  __extension__ unsigned long long int rdx;
  __extension__ unsigned long long int rsi;
  __extension__ unsigned long long int rdi;
  __extension__ unsigned long long int orig_rax;
  __extension__ unsigned long long int rip;
  __extension__ unsigned long long int cs;
  __extension__ unsigned long long int eflags;
  __extension__ unsigned long long int rsp;
  __extension__ unsigned long long int ss;
  __extension__ unsigned long long int fs_base;
  __extension__ unsigned long long int gs_base;
  __extension__ unsigned long long int ds;
  __extension__ unsigned long long int es;
  __extension__ unsigned long long int fs;
  __extension__ unsigned long long int gs;
};
#endif

void peek_and_output(pid_t pid, long long addr, long long size, int fd)
{
    // 標準出力か標準エラー出力
    if (fd != 1 && fd != 2) {
        return;
    }
    char *bytes = malloc(size + sizeof(long));
    int i;
    long data;

    for (i = 0 ; i < size ; i+= sizeof(long)){
        data = ptrace(PTRACE_PEEKDATA, pid, addr + i, 0);
        if (data == -1) {
                printf("failed to peek data\n");
                free(bytes);
                return;
        }
        memcpy(bytes + i, &data, sizeof(long));
    }
    bytes[size] = '\0';
    write(fd == 2 ? 2 : 1, bytes, size);
    fflush(fd == 2 ? stderr : stdout);
    free(bytes);
}

int main(int argc, char *argv[])
{
    int status;
    struct user_regs_struct regs;

    // コマンドライン引数で対象PIDを取得
    if (argc < 2) {
        fprintf(stderr, "Usage:  %s [PID]", argv[0]);
        exit(1);
    }

    // 対象PIDを指定
    pid_t pid = atoi(argv[1]);
    printf("attach to %d\n", pid);

    // アタッチ
    if (ptrace(PTRACE_ATTACH, pid, NULL, 0) < 0) {
        perror("failed to attach");
        exit(1);
    }

    ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACESYSGOOD);

    int is_enter_stop = 0;
    long prev_orig_rax = -1;
    while (1) {
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            break;
        } else if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            ptrace(PTRACE_GETREGS, pid, NULL, &regs);
            is_enter_stop = prev_orig_rax == regs.orig_rax ? !is_enter_stop : 1;
            prev_orig_rax = regs.orig_rax;
            if (is_enter_stop && regs.orig_rax == SYS_write) {
                peek_and_output(pid, regs.rsi, regs.rdx, (int)regs.rdi);
            }
        }
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
    }
//     // デタッチ
//     if (ptrace(PTRACE_DETACH, pid, 0, 0)< 0) {
//         perror("failed to detach");
//         exit(1);
//     }
//     printf("attached from %d\n",pid);

    return 0;
}
