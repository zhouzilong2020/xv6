// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
//
#include "param.h"
//
#include "memlayout.h"
//
#include "spinlock.h"
//
#include "riscv.h"
//
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[];  // first address after kernel.
                    // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 freecnt;
} kmem[NCPU];

void kinit() {
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem");
    kmem[i].freelist = 0;
    kmem[i].freecnt = 0;
  }

  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    kfree(p);
  }
  for (int i = 0; i < NCPU; i++) {
    printf("cpu-%d free: %d\n", i, kmem[i].freecnt);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  push_off();
  int hartid = cpuid();
  pop_off();

  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem[hartid].lock);

  r->next = kmem[hartid].freelist;
  kmem[hartid].freelist = r;
  ++kmem[hartid].freecnt;

  release(&kmem[hartid].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  push_off();
  int hartid = cpuid();
  pop_off();
  struct run *r;

  acquire(&kmem[hartid].lock);

  r = kmem[hartid].freelist;
  if (r) {
    kmem[hartid].freelist = r->next;
    --kmem[hartid].freecnt;
    release(&kmem[hartid].lock);
  } else { /* no more mem for the current hart's free list */
    release(&kmem[hartid].lock);
    int j;
    for (int i = 1; i < NCPU; i++) {
      if (hartid % 2 == 0)
        j = (hartid + i) % NCPU;
      else
        j = (hartid - i + NCPU) % NCPU;

      acquire(&kmem[j].lock);
      if (kmem[j].freecnt) {
        r = kmem[j].freelist;
        kmem[j].freelist = r->next;
        --kmem[j].freecnt;
      }
      release(&kmem[j].lock);

      if (r) break;
    }
  }

  if (r) memset((char *)r, 5, PGSIZE);  // fill with junk
  // else
  // panic("kalloc");

  return (void *)r;
}
