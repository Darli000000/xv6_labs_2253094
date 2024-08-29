// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// lab8
#define NBUCKET 13      // the count of hash table's buckets
#define HASH(blockno) (blockno % NBUCKET)

extern uint ticks;  // lab8

struct {
  struct spinlock lock;   // used for the buf alloc and size
  struct buf buf[NBUF];
  // lab8
  int size;     // record the count of used buf
  struct buf buckets[NBUCKET];  // lab8
  struct spinlock locks[NBUCKET];   // buckets' locks
  struct spinlock hashlock;     // the hash table's lock
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
//  struct buf head;    // lab8： delete
} bcache;

void
binit(void)
{
  int i;  //lab8
  struct buf *b;

  bcache.size = 0;  // lab8

  initlock(&bcache.lock, "bcache");

  initlock(&bcache.hashlock, "bcache_hash");    // lab8: init hash lock

  // lab8: 初始化所有bucket locks
  for(i = 0; i < NBUCKET; ++i) {
    initlock(&bcache.locks[i], "bcache_bucket");
  }

  // lab8 delete
  // Create linked list of buffers
  //bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    //b->next = bcache.head.next;
    //b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    //bcache.head.next->prev = b;
    //bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // lab8-2：通过 HASH 函数计算 blockno 对应的桶索引
  int idx = HASH(blockno);
  struct buf *pre, *minb = 0, *minpre;
  uint mintimestamp;
  int i;

  // 在对应的哈希桶中查找缓存的 buf
  acquire(&bcache.locks[idx]);  // lab8-2
  for(b = bcache.buckets[idx].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;  // 增加引用计数
      release(&bcache.locks[idx]);  // lab8-2
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 缓存未命中
  // 检查是否有未使用的 buf - lab8-2
  acquire(&bcache.lock);
  if(bcache.size < NBUF) {
    b = &bcache.buf[bcache.size++];
    release(&bcache.lock);
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->next = bcache.buckets[idx].next;  // 插入到哈希桶链表头部
    bcache.buckets[idx].next = b;
    release(&bcache.locks[idx]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock);
  release(&bcache.locks[idx]);

  // 根据时间戳选择哈希桶中最近最少使用的块 - lab8-2
  acquire(&bcache.hashlock);
  for(i = 0; i < NBUCKET; ++i) {
      mintimestamp = -1;  // 初始化最小时间戳为无穷大
      acquire(&bcache.locks[idx]);
      for(pre = &bcache.buckets[idx], b = pre->next; b; pre = b, b = b->next) {
          // 重新搜索块，防止竞争条件
          if(idx == HASH(blockno) && b->dev == dev && b->blockno == blockno){
              b->refcnt++;  // 增加引用计数
              release(&bcache.locks[idx]);
              release(&bcache.hashlock);
              acquiresleep(&b->lock);
              return b;
          }
          // 找到最近最少使用且未被引用的块
          if(b->refcnt == 0 && b->timestamp < mintimestamp) {
              minb = b;
              minpre = pre;
              mintimestamp = b->timestamp;
          }
      }
      // 找到未使用的块
      if(minb) {
          minb->dev = dev;
          minb->blockno = blockno;
          minb->valid = 0;
          minb->refcnt = 1;
          // 如果块在其他桶中，需要将其移动到正确的桶
          if(idx != HASH(blockno)) {
              minpre->next = minb->next;    // 从旧桶中移除
              release(&bcache.locks[idx]);
              idx = HASH(blockno);  // 计算正确的桶索引
              acquire(&bcache.locks[idx]);
              minb->next = bcache.buckets[idx].next;    // 将块移动到正确的桶
              bcache.buckets[idx].next = minb;
          }
          release(&bcache.locks[idx]);
          release(&bcache.hashlock);
          acquiresleep(&minb->lock);
          return minb;
      }
      release(&bcache.locks[idx]);
      if(++idx == NBUCKET) {
          idx = 0;  // 遍历下一个桶
      }
  }

  // 如果没有可用的 buf，触发 panic
  panic("bget: no buffers");
}


// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  int idx;  //lab8
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // lab8: change the lock
  idx = HASH(b->blockno);

  acquire(&bcache.locks[idx]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    //lab8: delete
    //b->next->prev = b->prev;
    //b->prev->next = b->next;
    //b->next = bcache.head.next;
    //b->prev = &bcache.head;
    //bcache.head.next->prev = b;
    //bcache.head.next = b;
    b->timestamp = ticks;  //lab8
  }
  
  release(&bcache.locks[idx]);
}

void
bpin(struct buf *b) {
  // lab8: change the bpin
  int idx = HASH(b->blockno);
  acquire(&bcache.locks[idx]);
  b->refcnt++;
  release(&bcache.locks[idx]);
}

void
bunpin(struct buf *b) {
  // lab8: change the bunpin
  int idx = HASH(b->blockno);
  acquire(&bcache.locks[idx]);
  b->refcnt--;
  release(&bcache.locks[idx]);
}


