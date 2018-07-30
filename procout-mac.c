#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //sleep
#include <sys/types.h> //caddr_t
#include <sys/ptrace.h>

#ifndef LINUX //Linux以外ではPTRACE_の代わりにPT_を使用
#if !defined(PTRACE_ATTACH) && defined(PT_ATTACHEXC)
#define PTRACE_ATTACH PT_ATTACHEXC
#endif
#if !defined(PTRACE_DETACH) && defined(PT_DETACH)
#define PTRACE_DETACH PT_DETACH
#endif
#endif

int main(int argc, char *argv[])
{
    // コマンドライン引数で対象PIDを取得
    if (argc < 2) {
        fprintf(stderr, "Usage:  %s [PID]", argv[0]);
        exit(1);
    }

    // 対象PIDを指定
    pid_t pid = atoi(argv[1]);
    printf("attach to %d\n", pid);

    // アタッチ
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) < 0) {
        perror("failed to attach");
        exit(1);
    }
    printf("attached to %d\n",pid);

    // テスト用スリープ
    sleep(5);

    // デタッチ
    if (ptrace(PTRACE_DETACH, pid, 0, 0)< 0) {
        perror("failed to detach");
        exit(1);
    }
    printf("attached from %d\n",pid);

    return 0;
}
