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

#define NBUCKET 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
  struct spinlock bklocks[NBUCKET];
  struct buf *buckets[NBUCKET];
} bcache;

extern uint ticks;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // init all buckets
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bklocks[i], "bucket lock");
    bcache.buckets[i] = 0;
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0];
    bcache.buckets[0] = b;
    initsleeplock(&b->lock, "buffer");
  }

  // printf("cpu %d finish binit\n", cpuid());
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache.lock);
  int bkid = blockno % NBUCKET;

  // printf("bget: dev: %d, bkid: %d\n", dev, bkid);

  acquire(&bcache.bklocks[bkid]);

  // Is the block already cached?
  for(b = bcache.buckets[bkid]; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      // find one
      b->refcnt++;
      b->t = ticks;
      // release(&bcache.lock);
      release(&bcache.bklocks[bkid]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached in the bucket,
  // first try to a LRU buf from current bucket
  struct buf *nb = 0;
  uint cur_ltime = 0xffffffff;
  for(b = bcache.buckets[bkid]; b != 0; b = b->next){
    if(b->refcnt == 0){
      // the first finded buf will certainly do this:
      if (b->t < cur_ltime) {
        nb = b;
        cur_ltime = b->t;
      } 
    }
  }

  if (nb) {
    nb->refcnt = 1;
    nb->t = ticks;
    nb->blockno = blockno;
    nb->dev = dev;
    nb->valid = 0;
    release(&bcache.bklocks[bkid]);
    acquiresleep(&nb->lock);
    return nb;
  }

  release(&bcache.bklocks[bkid]);


  // No free buf in current bucket
  // update search: lock up the whole bcache
  acquire(&bcache.lock);

  // check if the block cached again to avoid race
  for(b = bcache.buckets[bkid]; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->t = ticks;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // still not cached, search for LRU buf from all bucket
  cur_ltime = 0xffffffff;
  for (int i = 0; i < NBUCKET; i++) {
    for(b = bcache.buckets[i]; b != 0; b = b->next){
      if (b->refcnt == 0 && b->t < cur_ltime) {
        nb = b;
        cur_ltime = b->t;
      } 
    }
  }

  if (nb) {
    int bkid1 = nb->blockno % NBUCKET;
    if (bkid1 != bkid) {
      // remove from origin bucket
      if (nb == bcache.buckets[bkid1]) {
        bcache.buckets[bkid1] = nb->next;
      } else {
        for(b = bcache.buckets[bkid1]; b != 0; b = b->next) {
          if(b->next == nb) {
            b->next = nb->next;
            break;
          }
        }
      }
      // append to new bucket
      nb->next = bcache.buckets[bkid];
      bcache.buckets[bkid] = nb;
    }
    nb->dev = dev;
    nb->blockno = blockno;
    nb->valid = 0;
    nb->refcnt = 1;
    nb->t = ticks;
    release(&bcache.lock);
    acquiresleep(&nb->lock);
    return nb;
  }

  release(&bcache.lock);
  
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
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  b->refcnt--;
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


