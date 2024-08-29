// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  char lockname[8];     // lab6: 保存锁名
} kmems[NCPU];  // lab8: 给每个CPU上锁

void
kinit()
{
  // lab8：初始化kmem
  int i;
  for (i = 0; i < NCPU; ++i) {
    snprintf(kmems[i].lockname, 8, "kmem_%d", i);
    initlock(&kmems[i].lock, kmems[i].lockname);
  }
  
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int cpu_id;  //lab8: cpuid

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // lab8: 获取core number
  push_off();
  cpu_id = cpuid();
  pop_off();

  // lab8: 释放cpu页
  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

// lab8：从其他 CPU 的空闲内存页链表中偷取一半的页
struct run *steal(int cpu_id) {
  int i;
  int c = cpu_id;
  struct run *fast, *slow, *head;

  // 检查当前 CPU ID 是否与传入的 CPU ID 相同
  if (cpu_id != cpuid()) {
    panic("steal");
  }

  // 遍历其他 CPU 的空闲内存页链表
  for (i = 1; i < NCPU; ++i) {
    if (++c == NCPU) {
      c = 0;  // 如果到达最后一个 CPU，则从第一个 CPU 重新开始
    }
    
    // 获取目标 CPU 的内存池锁
    acquire(&kmems[c].lock);
    
    // 如果目标 CPU 的空闲内存页链表不为空
    if (kmems[c].freelist) {
      // 使用快慢指针法找到链表中间位置
      slow = head = kmems[c].freelist;
      fast = slow->next;
      while (fast) {
        fast = fast->next;
        if (fast) {
          slow = slow->next;
          fast = fast->next;
        }
      }
      
      // 从链表中间位置将后半部分偷走
      kmems[c].freelist = slow->next;
      release(&kmems[c].lock);  // 释放锁
      slow->next = 0;           // 断开链表，分离偷来的部分
      return head;              // 返回偷来的链表头指针
    }
    
    // 如果目标 CPU 的空闲链表为空，释放锁继续下一轮遍历
    release(&kmems[c].lock);
  }
  
  // 如果所有 CPU 都没有可偷取的内存页，返回 0
  return 0;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  // lab8：获取当前CPU的ID
  int c;
  push_off();
  c = cpuid();
  pop_off();

  // 从当前CPU的空闲内存页链表中获取一页内存
  acquire(&kmems[c].lock);
  r = kmems[c].freelist;
  if (r)
    kmems[c].freelist = r->next;
  release(&kmems[c].lock);

  // lab8-1：如果当前CPU没有空闲页，则从其他CPU“偷”一页
  if (!r && (r = steal(c))) {
    acquire(&kmems[c].lock);
    kmems[c].freelist = r->next;
    release(&kmems[c].lock);
  }

  // 如果成功分配到内存页，将其填充为垃圾数据
  if (r)
    memset((char*)r, 5, PGSIZE); // 填充为垃圾数据
  
  // 返回分配的内存页地址
  return (void*)r;
}