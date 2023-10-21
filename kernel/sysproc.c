#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
    uint64 base = 0; // represents a starting addr of page
    int size = 0; // number of pages the pgaccess detect
    uint64 bitmask_addr = 0; // represents a ptr
    argaddr(0, &base);
    argint(1, &size);
    argaddr(2, &bitmask_addr);

    unsigned int buffer = 0;
    struct proc* proc_p = myproc();
    pagetable_t pagetable = proc_p->pagetable;

    for (int i = 0; i < size; i++) {
        uint64 va = base + i * PGSIZE;
        pte_t* pte = walk(pagetable, va,0);
        if ((*pte) & PTE_A) {
            buffer |= 1 << (i);
        }
        *pte &= ~(1 << 6);
        
    }
    
    if (copyout(pagetable, bitmask_addr, (char*)&buffer, 4) == -1)
        return -1;

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
