#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXLEN 32  // 每个参数的最大长度

int main(int argc, char *argv[]) {
    char *path = "echo";            // 默认执行的命令路径
    char buf[MAXLEN * MAXARG] = {0}; // 用于存放输入的参数
    char *p = buf;                   // 指向当前参数的指针
    char *params[MAXARG];            // 存放参数指针的数组
    int oriParamCnt = 0;             // 初始参数个数（来自命令行参数）
    int paramIdx;                    // 当前参数在 params 数组中的索引
    int paramLen = 0;                // 当前参数的长度
    char ch;                         // 读取的字符
    int res;                         // read 返回值，判断输入是否结束

    // 处理命令行参数
    if (argc > 1) {
        path = argv[1];  // 第一个参数为要执行的命令
        for (int i = 1; i < argc; ++i) {
            params[oriParamCnt++] = argv[i];
        }
    } else {
        // 如果没有额外的参数，则默认执行 echo 命令
        params[oriParamCnt++] = path;
    }
    
    // 初始化参数索引
    paramIdx = oriParamCnt;

    // 循环读取输入
    while (1) {
        res = read(0, p, 1);  // 从标准输入读取一个字符
        ch = *p;

        if (res != 0 && ch != ' ' && ch != '\n') {
            // 如果读取的是非空字符，累加当前参数长度，并移动指针
            ++paramLen;
            ++p;
            continue;
        }

        // 当读取到空格或换行符，或输入结束时，将当前参数保存
        params[paramIdx++] = p - paramLen;
        *p = 0;  // 设置当前参数的结尾字符
        ++p;
        paramLen = 0;  // 重置参数长度

        // 如果参数数量达到上限，继续读取直到换行符
        if (paramIdx == MAXARG && ch == ' ') {
            while ((res = read(0, &ch, 1)) != 0) {
                if (ch == '\n') break;
            }
        }

        // 如果读取的是换行符或输入结束，执行命令
        if (ch != ' ') {
            if (fork() == 0) {
                exec(path, params);  // 子进程执行命令
                exit(0);
            } else {
                wait((int *) 0);  // 父进程等待子进程完成
                paramIdx = oriParamCnt;  // 重置参数索引
                p = buf;  // 重置缓冲区指针
            }
        }

        // 如果输入结束，退出循环
        if (res == 0) break;
    }
    
    exit(0);
}
