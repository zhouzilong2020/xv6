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
//
#include "param.h"
//
#include "spinlock.h"
//
#include "sleeplock.h"
//
#include "riscv.h"
//
#include "defs.h"
//
#include "fs.h"
//
#include "buf.h"

#define NBUCKET 7

struct bcache {
  struct spinlock lock;
  struct buf buf[NBUF];

  // circular double linked list
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache[NBUCKET];

void binit(void) {
  struct buf *b;
  struct bcache *bc;

  for (int i = 0; i < NBUCKET; i++) {
    bc = &bcache[i];
    initlock(&bc->lock, "bcache.bucket");

    // Create linked list of buffers
    bc->head.prev = &bc->head;
    bc->head.next = &bc->head;
    for (b = bc->buf; b < bc->buf + NBUF; b++) {
      b->next = bc->head.next;
      b->prev = &bc->head;
      initsleeplock(&b->lock, "buffer");
      bc->head.next->prev = b;
      bc->head.next = b;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;
  struct bcache *bc;

  bc = &bcache[blockno % NBUCKET];

  acquire(&bc->lock);

  // Is the block already cached?
  for (b = bc->head.next; b != &bc->head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bc->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bc->head.prev; b != &bc->head; b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bc->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock)) panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock)) panic("brelse");

  releasesleep(&b->lock);
  struct bcache *bc;
  bc = &bcache[b->blockno % NBUCKET];

  acquire(&bc->lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bc->head.next;
    b->prev = &bc->head;
    bc->head.next->prev = b;
    bc->head.next = b;
  }

  release(&bc->lock);
}

void bpin(struct buf *b) {
  struct bcache *bc;
  bc = &bcache[b->blockno % NBUCKET];

  acquire(&bc->lock);
  b->refcnt++;
  release(&bc->lock);
}

void bunpin(struct buf *b) {
  struct bcache *bc;
  bc = &bcache[b->blockno % NBUCKET];

  acquire(&bc->lock);
  b->refcnt--;
  release(&bc->lock);
}
