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

#define NBUFMAP_BUCKET 13//哈希表中的桶号索引，质数个可以降低哈希冲突的可能性
#define BUFMAP_HASH(dev, blockno) ((((dev)<<27) | (blockno)) % NBUFMAP_BUCKET)//哈希映射关系

struct {
  // struct spinlock lock;//保护整个缓冲缓存的锁
  struct buf buf[NBUF];//缓冲块数组，包含所有的缓冲区块
  struct spinlock eviction_lock;//驱逐锁

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct buf bufmap[NBUFMAP_BUCKET];//各桶的双向链表的头节点集合
  struct spinlock bufmap_locks[NBUFMAP_BUCKET];//桶锁
} bcache;

void
binit(void)
{
  //初始化桶锁
  for(int i = 0; i < NBUFMAP_BUCKET; i++) {
    initlock(&bcache.bufmap_locks[i], "bcache_bufmap");
    bcache.bufmap[i].next = 0;
  }

  for(int i = 0; i < NBUF; i++) {
    //初始化缓存区块
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->lastuse = 0;//初始未被使用
    b->refcnt = 0;//初始引用计数为0

    //将所有缓冲区块使用头插法添加到bufmap[0]
    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }
  initlock(&bcache.eviction_lock, "bcache_eviction");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  //哈希获取桶号
  uint key = BUFMAP_HASH(dev, blockno);

  acquire(&bcache.bufmap_locks[key]);//获取桶锁

  // Is the block already cached?
  //遍历该桶中双链表查询blockno的缓冲区块是否已经在缓冲区中
  for(b = bcache.bufmap[key].next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){//检查设备号和块号，若一致
      b->refcnt++;//增加引用计数，表示新增一个进程引用此缓冲区块
      release(&bcache.bufmap_locks[key]);//释放桶锁
      acquiresleep(&b->lock);//获取缓冲块的睡眠锁
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //不在缓冲区
  //为防止死锁，先释放当前桶锁
  release(&bcache.bufmap_locks[key]);
  //为防止blockno的缓冲区块被重复创建，加驱逐锁
  acquire(&bcache.eviction_lock);

  //释放桶锁：加驱逐锁的间隙可能有其他进程创建了blockno的缓冲区块，因此再检查一次
  for(b = bcache.bufmap[key].next; b != 0; b = b->next){//未缓存则根据LRU从尾部开始寻找可替换的缓冲块
    if(b->dev == dev && b->blockno == blockno) {
      acquire(&bcache.bufmap_locks[key]);//添加引用计数时加桶锁
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      release(&bcache.eviction_lock);
      acquiresleep(&b->lock);//加睡眠锁
      return b;
    }
  }
  //仍不在缓冲区（加驱逐锁之前该块还没有被创建）
  //此时是持有驱逐锁，不持有任何桶锁，查询所有桶中的LRU-buf
  struct buf *before_least = 0;//LRU-buf的前一个块
  uint holding_bucket = -1;//记录当前持有哪个桶锁

  //循环查询所有桶
  for(int i = 0; i < NBUFMAP_BUCKET; i++){

    acquire(&bcache.bufmap_locks[i]);
    int newfound = 0;//是否在当前桶找到新的LRU-buf
    //遍历该桶中链表
    for(b = &bcache.bufmap[i]; b->next; b = b->next) {//这里遍历的是前驱块
      if(b->next->refcnt == 0 && (!before_least || b->next->lastuse < before_least->next->lastuse)) {
        before_least = b;
        newfound = 1;
      }
    }
    if(!newfound)
      release(&bcache.bufmap_locks[i]);
    else {//找到了新的LRU-buf
      if(holding_bucket != -1)//若当前找到的不是第一个LRU-buf，之前肯定持有某个桶锁，需要释放
        release(&bcache.bufmap_locks[holding_bucket]);
      holding_bucket = i;//把标记holding_bucket更改为当前桶锁编号
    }
  }
  //若没有找到任何一个LRU-buf，表示没有空闲缓存块了
  if(!before_least)
    panic("bget: no buffers");
  b = before_least->next;
  if(holding_bucket != key) {
    before_least->next = b->next;
    release(&bcache.bufmap_locks[holding_bucket]);
    //将LRU-buf添加到key桶
    acquire(&bcache.bufmap_locks[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }
  //设置新buf的字段
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  //释放相关锁
  release(&bcache.bufmap_locks[key]);
  release(&bcache.eviction_lock);
  acquiresleep(&b->lock);
  return b;
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
brelse(struct buf *b)//释放锁并更新引用计数
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = BUFMAP_HASH(b->dev, b->blockno);
  
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->lastuse = ticks;
  }
  
  release(&bcache.bufmap_locks[key]);
}

void
bpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void
bunpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}


