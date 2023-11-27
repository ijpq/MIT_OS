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
} kmem;

// Define the reference count array and lock.
struct spinlock ref_lock;
uint8 reference_cnt[((uint64)(PHYSTOP - KERNBASE)) / PGSIZE];

// Init the reference count array.
void init_ref() {
  initlock(&ref_lock, "reference pa");
  for (int i = 0; i < (PHYSTOP - KERNBASE)/PGSIZE; i++) {
    reference_cnt[i] = 0;
  }
}

void incr_ref(uint64 pa) {
  acquire(&ref_lock);
  if (pa < KERNBASE || pa > PHYSTOP) {
    panic("incr");
    exit(-1);
  }
  reference_cnt[(pa-KERNBASE)/PGSIZE] += 1;
  release(&ref_lock);
}

void decr_ref(uint64 pa) {
  acquire(&ref_lock);
  if (pa < KERNBASE || pa > PHYSTOP) {
    panic("decr");
    exit(-1);
  }
  reference_cnt[(pa-KERNBASE)/PGSIZE] -= 1;
  release(&ref_lock);
}

void set_ref(uint64 pa, int v) {
  if (pa < KERNBASE || pa > PHYSTOP) {
    panic("set ref");
    exit(-1);
  }
  acquire(&ref_lock);
  reference_cnt[(pa-KERNBASE)/PGSIZE] = v;
  release(&ref_lock);
}

uint8 get_ref(uint64 pa) {
  if (pa < KERNBASE || pa > PHYSTOP) {
    panic("read ref");
    exit(-1);
  }
  uint8 ret;
  acquire(&ref_lock);
  ret = reference_cnt[(pa-KERNBASE)/PGSIZE];
  release(&ref_lock);
  return ret;
}

struct spinlock*
get_ref_lock() {
  return &ref_lock;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  init_ref(); 
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
  // kfree的调用处:
  // 1. uvmunmap
  // 2. freerange <- init
  // 3. mappages failed
  // 4. arbitary
  // 目前，总会de ref.
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) {
    panic("kfree");
  }

  if (is_runtime_pa((uint64)pa)) {
    uint8 ref = get_ref((uint64)pa);
    if (ref > 1) {
      set_ref((uint64)pa, ref-1);
      return ;
    }
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
  if (is_runtime_pa((uint64)pa))
    set_ref((uint64)pa, 0);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

#include "proc.h"
int cow(struct proc* p, uint64 va) {
    pagetable_t usr_pgtbl = p->pagetable;
    pte_t *usr_pte = walk(usr_pgtbl, va, 0);
    if (usr_pte == 0) {
        panic("usertrap: cannot walk");
        exit(-1);
    }

    int flags = PTE_FLAGS(*usr_pte);
    if (flags & PTE_COW) {
        char* mem;
        if ((mem = kalloc()) == 0) {
            printf("usertrap: cannot kalloc\n");
            return -1;
        }
        memmove(mem, (char*)PTE2PA(*usr_pte), PGSIZE);

        flags |= PTE_W; // set write flag
        flags &= ~PTE_COW; // clear cow
        uvmunmap(usr_pgtbl, PGROUNDDOWN(va), 1, 1); // do_free是为了去掉旧pa的一个ref
        if (mappages(usr_pgtbl, va, PGSIZE - (va % PGSIZE), (uint64)mem, flags) != 0) {
            kfree(mem);
            panic("cannot map to new pa");
        }
    }
    return 0;
}