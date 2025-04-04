//由于fork创建子进程的操作会把父进程的文件描述符也都复制给子进程
//为了节省文件描述符，需要及时关闭所有管道两个方向中用不到的文件描述符
//每个进程只从左边的管道读，只写右边的管道，可以将左侧管道的写描述符和右侧管道的读描述符关闭
//管道的读端或写端有一个引用计数，只有当所有指向写端的文件描述符都关闭时，写端才会真正被关闭，子进程关闭它的写端只是减少了引用计数

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//筛选质数的函数，接收一个管道作为参数
void sieve(int pleft[2]) {
    //从左邻居读取整数
    int p;
    if(read(pleft[0], &p, sizeof(p)) != sizeof(p)) {
        fprintf(2, "read error.\n");
        exit(1);
    }
    if(p == -1) {//读取到-1，表示结束
        exit(0);
    }
    printf("prime %d\n", p);//此时接收到的数字肯定是质数
    
    //创建一个新的管道
    int pright[2];
    pipe(pright);

    if(fork() == 0) {//是子进程，也就是新产生的右邻居
        close(pright[1]);//右邻居用不到写端
        close(pleft[0]);//左邻居用不到读端
        sieve(pright);//递归调用筛选函数
    }
    else {//是父进程
        close(pright[0]);//父进程用不到右邻居的读管道
        int buf;//从左邻居接收数字
        while(read(pleft[0], &buf, sizeof(buf)) && buf != -1) {
            if(buf % p != 0) {//接受到的不是上一个数字的倍数
                write(pright[1], &buf, sizeof(buf));//发送给右邻居
            }
        }
        //若接收到左邻居发送的-1， 传递给右邻居
        buf = -1;
        write(pright[1], &buf, sizeof(buf));
        wait(0);
        exit(0);
    }
}

int main(int argc, char **argv) {
    //创建初始管道
    int input_pipe[2];
    pipe(input_pipe);

    if(fork() == 0) {//是子进程，也就是右邻居
        close(input_pipe[1]);
        sieve(input_pipe);
        exit(0);
    }
    else {
        close(input_pipe[0]);
        int i;
        for(i = 2; i <= 35; i++) {
            write(input_pipe[1], &i, sizeof(i));//依次向管道写入待筛选数字
        }
        //写入结束标志
        i = -1;
        write(input_pipe[1], &i, sizeof(i));
    }
    wait(0);//等待子进程结束

    exit(0);
}
