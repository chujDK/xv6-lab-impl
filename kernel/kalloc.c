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
} kmem, kmem_percpu[NCPU];

struct spinlock shrink_lock;

void
kinit()
{
  char lock_name[16];

  initlock(&kmem.lock, "kmem");

  for (int i = 0; i < NCPU; i++) {
    snprintf(lock_name, sizeof(lock_name), "kmem_%d", i);
    initlock(&kmem_percpu[i].lock, lock_name);
  }

  initlock(&shrink_lock, "kmem_shrink");

  freerange(end, (void*)PHYSTOP);
}

void
kfree_internal(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree_internal");

  r = (struct run*)pa;

  memset(pa, 1, PGSIZE);

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_internal(p);
}

void
shrink_cpu_freelist(int cpuid)
{
  struct run* tmp;

  acquire(&kmem_percpu[cpuid].lock);
  for (struct run* p = kmem_percpu[cpuid].freelist; p != 0;) {
    tmp = p;
    p = p->next;
    kfree_internal(tmp);
  }
  kmem_percpu[cpuid].freelist = 0;
  release(&kmem_percpu[cpuid].lock);
}

void
shrink_free(int caller_cpuid)
{
  acquire(&shrink_lock);
  for (int i = 0; i < NCPU; i++) {
    if (i != caller_cpuid) {
      shrink_cpu_freelist(i);
    }
  }
  release(&shrink_lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int mycpu;
  push_off();
  mycpu = cpuid();

  if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem_percpu[mycpu].lock);
  r->next = kmem_percpu[mycpu].freelist;
  kmem_percpu[mycpu].freelist = r;
  release(&kmem_percpu[mycpu].lock);

  pop_off();
}

void *
kalloc_internal(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int mycpu;

  push_off();
  mycpu = cpuid();

  acquire(&kmem_percpu[mycpu].lock);
  r = kmem_percpu[mycpu].freelist;
  if (r) {
    kmem_percpu[mycpu].freelist = r->next;
  } else {
    // try to alloc from internal freelist
    r = kalloc_internal();
    if (!r) {
      // failed, try to steal from other cpus
      shrink_free(mycpu);
      r = kalloc_internal();
    }
  }

  release(&kmem_percpu[mycpu].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void*)r;
}
