#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int target_pid;
    long ret;

    target_pid = atoi(argv[1]);
    printf("attach to %i \n", target_pid);

    ret = ptrace (PT_ATTACH, target_pid, NULL, 0);
    printf("attach %lu \n", ret);

    sleep(5);

    ret = ptrace (PT_CONTINUE, target_pid, NULL, 0);
    printf("continue %lu \n", ret);

    sleep(5);

    ret = ptrace (PT_DETACH, target_pid, NULL, 0);
    printf("detach %lu \n", ret);
}
