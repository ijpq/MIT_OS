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

struct kmem{
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct kmem kmem0,kmem1,kmem2,kmem3,kmem4,kmem5,kmem6,kmem7;

struct kmem* mem_list[NCPU] = {&kmem0, &kmem1, &kmem2, &kmem3, &kmem4, &kmem5, &kmem6, &kmem7};

void
kinit()
{
  // initlock(&kmem.lock, "kmem");
  for (int i = 0; i < NCPU; i++) {
    char name[5];
    memmove(name, "kmem", 4);
    name[4] = (char)0 + i;
    initlock(&mem_list[i]->lock, name);
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();
  int id = cpuid();

  acquire(&mem_list[id]->lock);
  r->next = mem_list[id]->freelist;
  mem_list[id]->freelist = r;
  release(&mem_list[id]->lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id = cpuid();

  acquire(&mem_list[id]->lock);
  r = mem_list[id]->freelist;
  if(r) {
    mem_list[id]->freelist = r->next;
  } else {
    // current mem list empty, steal.
    for (int i = 0; i < NCPU; i++) {
      if (i == id) continue;
      acquire(&mem_list[i]->lock);
      r = mem_list[i]->freelist;
      if (!r) {
        release(&mem_list[i]->lock);
      } else {
        mem_list[i]->freelist = r->next;
        release(&mem_list[i]->lock);
        break;
      }
    }
  }
  release(&mem_list[id]->lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  
  pop_off();
  return (void*)r;
}
