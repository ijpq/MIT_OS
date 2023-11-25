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
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) {
    printf("mod pgsize: %d\n", (uint64)pa % PGSIZE);
    printf("lower than end: %d\n", (char*)pa < end);
    printf("large eq than phystop: %d\n", (uint64)pa >= PHYSTOP);
    printf("pa %p\n", pa);
    panic("kfree");
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
void cow(struct proc* p, uint64 va) {
    pagetable_t usr_pgtbl = p->pagetable;
    pte_t *usr_pte = walk(usr_pgtbl, va, 0);
    if (usr_pte == 0) {
        panic("usertrap: cannot walk");
        exit(-1);
    }

    int flags = PTE_FLAGS(*usr_pte);
    printf("detect pte: %p whether cow\n", usr_pte);
    if (flags & PTE_COW) {
        char* mem;
        if ((mem = kalloc()) == 0) {
            // TODO: kill the proc
            p->killed = 1;
            panic("usertrap: cannot kalloc");
        }
        memmove(mem, (char*)PTE2PA(*usr_pte), PGSIZE);
        printf("pte %p is cow, change from pa %p to pa %p\n", usr_pte, mem, PTE2PA(*usr_pte));

#if 0
        decr_ref((uint64)PTE2PA(*usr_pte));
        if (get_ref(PTE2PA(*usr_pte)) == 0) {
          kfree((char*)PTE2PA(*usr_pte));
        }
        incr_ref((uint64)mem);
        pte_t newpte = PA2PTE(mem);
        newpte = ((newpte | PTE_FLAGS(*usr_pte)) | PTE_W) & ~PTE_COW;
        *usr_pte = newpte;
#endif
        flags &= PTE_W;
        flags |= ~PTE_COW;
        uvmunmap(usr_pgtbl, PGROUNDDOWN(va), 1, 1);
        // after uvmunmap, the pte would be zero

        if (mappages(usr_pgtbl, va, PGSIZE, (uint64)mem, flags) != 0) {
            kfree(mem);
            panic("cannot map to new pa");
        }
    }
}