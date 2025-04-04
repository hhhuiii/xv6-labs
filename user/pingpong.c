#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char **argv) {
    int pp2c[2], pc2p[2];
    if (pipe(pp2c) < 0 || pipe(pc2p) < 0) {
        fprintf(2, "pipe failed\n");
        exit(1);
    }

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid != 0) { // 是父进程
        close(pp2c[0]);
        close(pc2p[1]);//不需要子进程方的读端和写端
        if (write(pp2c[1], "P", 1) != 1) {
            fprintf(2, "parent write failed\n");
            exit(1);
        }
        close(pp2c[1]); // 在写之后立即关闭父进程的写段，以免管道读端进程阻塞等待后续根本不存在的数据导致进程永远阻塞

        char bufp;
        if (read(pc2p[0], &bufp, 1) != 1) {
            fprintf(2, "parent read failed\n");
            exit(1);
        }
        fprintf(1, "%d: received pong\n", getpid());
        close(pc2p[0]);
        wait(0);
    } else { // 是子进程
        close(pc2p[0]);
        close(pp2c[1]);//不需要父进程方的读端和写端
        char bufc;
        if (read(pp2c[0], &bufc, 1) != 1) {
            fprintf(2, "child read failed\n");
            exit(1);
        }
        fprintf(1, "%d: received ping\n", getpid());
        close(pp2c[0]); // 读后关闭读端

        if (write(pc2p[1], &bufc, 1) != 1) {
            fprintf(2, "child write failed\n");
            exit(1);
        }
        close(pc2p[1]); // 写后立即关闭写端
    }
    exit(0);
}