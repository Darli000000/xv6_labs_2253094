#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("usage:sleep[ticks num]\n");
        exit(1);
    }
    sleep(atoi(argv[1]));
    printf("(nothing happens for a little while)\n");
    exit(0);
}