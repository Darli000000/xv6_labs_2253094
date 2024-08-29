#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int subProcess(int *oldFd) {
    // 关闭旧管道的写端
    close(oldFd[1]);

    int fd[2];      // 新管道
    int prime;      // 当前质数
    int num;        // 从管道中读取的数

    // 尝试从旧管道中读取第一个数（即质数）
    if (read(oldFd[0], &prime, sizeof(prime))) {
        // 第一个读取到的数是质数，输出它
        printf("prime %d\n", prime);

        // 创建一个新管道用于下一步筛选
        pipe(fd);

        // 创建子进程来处理后续的筛选
        if (fork() == 0) {
            // 子进程递归调用 subProcess 继续筛选质数
            subProcess(fd);
        } else {
            // 父进程关闭新管道的读端
            close(fd[0]);

            // 从旧管道读取数并筛选
            while (read(oldFd[0], &num, sizeof(num))) {
                // 如果当前数不能被当前质数整除，将其写入新管道
                if (num % prime != 0) {
                    write(fd[1], &num, sizeof(num));
                }
            }

            // 父进程关闭旧管道的读端
            close(oldFd[0]);
            // 关闭新管道的写端
            close(fd[1]);
            // 等待子进程结束
            wait((int *) 0);
        }
    } else {
        // 如果旧管道中没有数据可读，关闭旧管道的读端
        close(oldFd[0]);
    }

    // 退出当前进程
    exit(0);
}

int main() {
    int i;
    int fd[2];

    // 创建初始管道
    pipe(fd);

    // 创建子进程进行筛选
    if (fork() == 0) {
        subProcess(fd);
    } else {
        // 父进程关闭管道的读端
        close(fd[0]);

        // 将 2 到 35 的数写入管道
        for (i = 2; i <= 35; ++i) {
            write(fd[1], &i, sizeof(i));
        }

        // 写入完毕后关闭管道的写端，并等待子进程结束
        close(fd[1]);
        wait((int *) 0);
    }

    // 退出主进程
    exit(0);
}