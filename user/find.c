#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 在指定目录中查找文件
void find(char *dir, char *file) {
    char buf[512] = {0}, *p;  // 存储路径的缓冲区和指向缓冲区的指针
    int fd;
    struct dirent de;  // 目录项结构体
    struct stat st;    // 文件状态结构体
    
    // 打开目录，获取文件描述符
    if ((fd = open(dir, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", dir);  // 打开目录失败，输出错误信息
        return;
    }

    // 检查路径长度是否超过缓冲区大小
    if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof(buf)) {
        fprintf(2, "find: path too long\n");  // 路径过长，输出错误信息
        close(fd);
        return;
    }

    strcpy(buf, dir);          // 将目录路径复制到缓冲区
    p = buf + strlen(buf);     // 将指针移动到路径的末尾
    *p++ = '/';                // 在路径末尾添加斜杠

    // 遍历目录下的所有项
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {  // 从目录中读取一个目录项
        // 跳过空目录项、当前目录和上级目录
        if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
            continue;
        }

        // 将目录项的名称附加到路径中，并添加字符串结束符
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        // 获取路径对应文件的状态信息
        if (stat(buf, &st) < 0) {
            fprintf(2, "find: cannot stat %s\n", buf);  // 获取状态失败，输出错误信息
            continue;
        }

        // 如果是目录，则递归地调用 find 函数继续查找
        if (st.type == T_DIR) {
            find(buf, file);
        } 
        // 如果是文件且名称与目标文件名匹配，则输出完整路径
        else if (strcmp(de.name, file) == 0) {
            printf("%s\n", buf);
        }
    }

    close(fd);  // 关闭目录文件描述符
    return;
}

int main(int argc, char *argv[]) {
    struct stat st;

    // 检查命令行参数的个数是否为 3
    if (argc != 3) {
        fprintf(2, "Usage: find dir file\n");  // 参数错误，输出使用方法
        exit(1);
    }

    // 获取指定目录的状态信息并检查是否成功
    if (stat(argv[1], &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", argv[1]);  // 获取状态失败，输出错误信息
        exit(2);
    }

    // 检查指定路径是否为目录
    if (st.type != T_DIR) {
        fprintf(2, "find: '%s' is not a directory\n", argv[1]);  // 不是目录，输出错误信息
        exit(3);
    }

    // 调用 find 函数查找文件
    find(argv[1], argv[2]);
    exit(0);  // 成功退出
}
