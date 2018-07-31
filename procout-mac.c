#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //sleep
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
        data = ptrace(PTRACE_PEEKDATA, pid, (caddr_t)addr + i, 0);
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
    pid_t pid;
    int status;

#ifndef __APPLE__
    struct user_regs_struct regs;
#else
    x86_thread_state_t regs;
    mach_port_t task; // macOSの処理単位
#endif

    // コマンドライン引数で対象PIDを取得
    if (argc < 2) {
        fprintf(stderr, "Usage:  %s [PID]", argv[0]);
        exit(1);
    }

    // 対象PIDを指定
    pid = atoi(argv[1]);
    printf("attach to %d\n", pid);

    // アタッチ
    if (ptrace(PTRACE_ATTACH, pid, NULL, 0) < 0) {
        perror("ptrace() failed to attach");
        exit(1);
    }
    printf("attached to %d\n", pid);

#ifndef __APPLE__
    ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACESYSGOOD);
    int is_enter_stop = 0;
    long prev_orig_rax = -1;
#endif

    while (1) {
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            break;
        } else if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
#ifndef __APPLE__
            ptrace(PTRACE_GETREGS, pid, NULL, &regs);
            is_enter_stop = prev_orig_rax == regs.orig_rax ? !is_enter_stop : 1;
            prev_orig_rax = regs.orig_rax;
            if (is_enter_stop && regs.orig_rax == SYS_write) {
                // PTRACE_PEEKDATAによる読み取り
                peek_and_output(pid, regs.rsi, regs.rdx, (int)regs.rdi);
            }
        }
        // 再開し，次のシステムコールで再度停止する処理
        ptrace(PTRACE_SYSCALL, pid, NULL, 0);
#else
            // タスクのPID（対象）を指定
            if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) {
                perror("task_for_pid() failed\n");
                exit(EXIT_FAILURE);
            }

            // タスクの中断（レジスタ値取得のため）
            if (task_suspend(task) != KERN_SUCCESS) {
                perror("task_suspend() failed\n");
                exit(EXIT_FAILURE);
            }

            // スレッドの確認
            thread_act_array_t threads = NULL;
            mach_msg_type_number_t threadCount;
            if (task_threads(task, &threads, &threadCount) != KERN_SUCCESS) {
                perror("task_threads() failed\n");
                exit(EXIT_FAILURE);
            }

            // レジスタ値取得
            mach_msg_type_number_t count = x86_THREAD_STATE_COUNT;
            if (thread_get_state(threads[0], x86_THREAD_STATE, (thread_state_t)&regs, &count) != KERN_SUCCESS) {
                perror("thread_get_state() failed\n");
                exit(EXIT_FAILURE);
            }
            printf("RSI = %llx\n", regs.uts.ts64.__rsi);

            // PTRACE_PEEKDATAによる読み取り
            peek_and_output(pid, regs.uts.ts64.__rsi, regs.uts.ts64.__rdx, (int)regs.uts.ts64.__rdi);

             // タスクの再開
            if (task_resume(task) != KERN_SUCCESS) {
                perror("task_resume() failed\n");
                exit(EXIT_FAILURE);
            }

            mach_port_deallocate(mach_task_self(), task);
            exit(EXIT_SUCCESS);
        }
        // 再開し，次のシステムコールで再度停止する処理
        // ptrace(PTRACE_SYSCALL, pid, NULL, 0);相当のものを入れる
#endif
    }

    // デタッチ
    if (ptrace(PTRACE_DETACH, pid, 0, 0)< 0) {
        perror("ptrace() failed to detach");
        exit(1);
    }
    printf("detached from %d\n",pid);

    return 0;
}
