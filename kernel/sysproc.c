#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

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

uint64
sys_trace(void) 
{
  int trace_mask = 0;
  if (argint(0, &trace_mask) < 0) {
    return -1;
  }
  struct proc *p = myproc();
  p->trace_mask = trace_mask;
  return 0;
}

// TODO: 1. 这个ptr应该怎么声明？现在这种声明方式是抄的fstat，为什么直接声明为一个整数呢？fstat传进来的也是一个struct stat*。如果声明为struct sysinfo *，那么需要include?
uint64 
sys_info(void) {  // 内核函数 sysinfo
  uint64 ptr; // 用户态的sysinfo函数会接受一个指向struct sysinfo的指针，在kernel中使用argaddr来将这个用户态的指针拷贝到ptr中。这个ptr就是一个指针，所以是一个uint64的整数值。
  if(argaddr(0, (void*)&ptr) < 0) {  // 可以看下这个argaddr的定义，就是把参数寄存器的值返回来出来。
    printf("argaddr failed\n");
    return -1;
  }
  
  // collect amount of free mem
  uint64 memcount = kcountfreemem();
  // collect number of process
  uint64 proccount = kcountfreeproc();
  // set to struct sysinfo
  struct sysinfo info;
  info.freemem = memcount;
  info.nproc = proccount;
  // copyout
  struct proc *p = myproc();
  
  if(copyout(p->pagetable, ptr, (char*)&info, sizeof(info)) < 0) { // copyout会把info结构体的内容拷贝到ptr所指向的内存。ptr是用户态给出的内存地址，info结构体是内核态写好的内容。因此就完成了内核向用户空间的数据拷贝
    printf("copyout failed\n");
    return -1;
  }
  return 0; }