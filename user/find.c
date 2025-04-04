#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

//递归查找函数，查找路径为path的目录下是否有目标文件target
void find(char *path, char *target) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    //打开目录
    if((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    //获取目录的状态信息
    if(fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    //文件目录区别处理
    switch(st.type) {
    case T_FILE:
        if(strcmp(path + strlen(path) - strlen(target), target) == 0) {
            printf("%s\n", path);
        }
        break;
    case T_DIR:
        //检查路径长度是否超出缓冲区大小
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        //读取目录项
        while(read(fd, &de, sizeof(de)) == sizeof(de)) {
            if(de.inum == 0) {
                continue;
            }
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            //获取目录项的状态信息
            if(stat(buf, &st) < 0) {//st存储获取到的文件属性信息
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            //排除.和..目录
            if(strcmp(buf + strlen(buf) - 2, "/.") != 0 && strcmp(buf + strlen(buf) - 3, "/..") != 0) {
                find(buf, target);//递归查找子目录
            }
        }
        break;
    }
    close(fd);
}

int main(int argc, char **argv) {
    if(argc < 3) {
        exit(0);
    }
    char target[512];
    target[0] = '/';
    strcpy(target + 1, argv[2]);
    find(argv[1], target);
    exit(0);
}