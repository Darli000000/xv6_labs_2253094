#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"  // lab2

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64  // lab2的trace函数
sys_trace(void) {
    int mask;
    // 获取整型的系统调用参数
    if (argint(0, &mask) < 0) {
        return -1;
    }
    // 存入proc结构体的mask变量中
    myproc()->mask = mask;
    return 0;
}

// lab2系统调用：获取系统信息
uint64 sys_sysinfo(void) {
    uint64 info_addr;          // 用户态下的 sysinfo 结构体地址
    struct sysinfo info;       // 用于存储系统信息的结构体

    // 获取用户传递的参数，即 sysinfo 结构体的地址
    if (argaddr(0, &info_addr) < 0) {
        return -1;  // 获取地址失败，返回错误
    }

    // 获取系统中可用的空闲内存和当前运行的进程数量
    info.freemem = getfreemem();  // 获取系统空闲内存
    info.nproc = getnproc();      // 获取当前运行的进程数量

    // 将内核态中的 sysinfo 结构体数据复制到用户态指定的地址
    if (copyout(myproc()->pagetable, info_addr, (char *) &info, sizeof(info)) < 0) {
        return -1;  // 复制失败，返回错误
    }

    return 0;  // 系统调用成功，返回 0
}
