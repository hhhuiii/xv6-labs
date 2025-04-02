#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char **argv) {
    if(argc < 2) {
        printf("usage : sleep <ticks>\n");
    }
    sleep(atoi(argv[1]));//stoi是C++专属
    exit(0);
}