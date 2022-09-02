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

#define BHASHSIZE 13
struct bhashtbl {
  struct spinlock lock;
  struct buf head;
};

static
int hash(uint dev, uint blockno)
{
  return (((uint64)dev << 32) | (uint64) blockno) % BHASHSIZE;
}

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct bhashtbl hash[BHASHSIZE];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i = 0; i < BHASHSIZE; i++){
    initlock(&bcache.hash[i].lock, "bcache_bhashtbl");
    bcache.hash[i].head.prev = &bcache.hash[i].head;
    bcache.hash[i].head.next = &bcache.hash[i].head;
  }

  // add all buffers to hash[0]
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hash[0].head.next;
    b->prev = &bcache.hash[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.hash[0].head.next->prev = b;
    bcache.hash[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int hashid = hash(dev, blockno);
  struct bhashtbl* bucket = &bcache.hash[hashid];
  acquire(&bucket->lock);

  for(b = bucket->head.next; b != &bucket->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucket->lock);

  acquire(&bcache.lock);

  // Not cached.
  // Recycle all hashtbl and get the least recent buf
  uint least_recent_time_stamp = 0;
  least_recent_time_stamp--;
  struct buf* least_recent_buf = 0;
  for(int hidx = 0; hidx < BHASHSIZE; hidx++){
    acquire(&bcache.hash[hidx].lock);
    for(b = bcache.hash[hidx].head.prev; b != &bcache.hash[hidx].head; b = b->prev){
      if(b->refcnt == 0){
        if(b->timestamp <= least_recent_time_stamp){
          least_recent_buf = b;
          least_recent_time_stamp = b->timestamp;
        }
      }
    }
  }

  if(least_recent_buf == 0){
    panic("bget: no buffers");
  }

  b = least_recent_buf;
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  // unlink
  b->next->prev = b->prev;
  b->prev->next = b->next;

  // link
  b->next = bucket->head.next;
  b->prev = &bucket->head;
  bucket->head.next->prev = b;
  bucket->head.next = b;
  for(int hidx = 0; hidx < BHASHSIZE; hidx++){
    release(&bcache.hash[hidx].lock);
  }

  release(&bcache.lock);
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
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int hashid = hash(b->dev, b->blockno);
  struct bhashtbl* bucket = &bcache.hash[hashid];
  acquire(&bucket->lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // update timestamp
    b->timestamp = ticks;
  }
  
  release(&bucket->lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


