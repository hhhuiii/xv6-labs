#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

//运行指定的程序，接收参数
void run(char *program, char **args) {
    //创建子进程，在子进程中执行指定的程序
    if(fork() == 0) {
        exec(program, args);
        exit(0);
    }
    return;
}

int main(int argc, char **argv) {
    char buf[2048];//存储从标准输入读取的数据
    char *p = buf, //寻找参数分隔符
    *last_p = buf;//记录当前参数的起始位置
    char *argsbuf[128];//存储所有参数的字符串数组
    char **args = argsbuf;//用于填充argsbuf的指针

    //首先将xargs的参数复制到argsbuf中
    for(int i = 1; i < argc; i++) {
        *args++ = argv[i];
    }
    //记录当前参数位置
    char ** pa = args;
    //从标准输入读取数据，存储在缓冲区中
    while(read(0, p, 1) != 0) {
        //使用p遍历缓冲区，遇到空格或换行符时，将其替换为字符串结束符\0
        if(*p == ' ' || *p == '\n') {
            *p = '\0';
            //将参数添加到argsbuf中
            *pa++ = last_p;
            last_p = p + 1;
            //遇到换行符时，表示一组参数读取完毕，调用run函数执行程序，传递参数
            if(*p == '\n') {
                *pa = 0;//exec要求参数列表以NULL结尾
                run(argv[1], argsbuf);
                pa = args;
            }
        }
        //继续读取数据
        p++;
    }
    //如果最后一行不是空行，同样的逻辑再处理依次
    if(pa != args) {
        *p = '\0';
        *pa++ = last_p;
        *pa = 0;

        run(argv[1], argsbuf);
    }
    //等待所有子进程结束
    while(wait(0) != -1);
    exit(0);
}