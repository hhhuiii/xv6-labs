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
  int mask;

  if(argint(0, &mask) < 0)  // 尝试从用户空间获取索引为0的第一个参数复制给当前进程属性
    return -1;
  myproc()->kama_syscall_trace = mask;  // 参考getpid函数的实现，myproc()通过访问当前CPU的进程指针来获取当前进程
  return 0;
}

uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  freebytes(&info.freemem);  // 获取空闲内存字节数
  procnum(&info.nproc);  // 获取进程数量

  // 获取用户虚拟地址
  uint64 dstaddr;
  argaddr(0, &dstaddr);  // 从系统调用的参数列表中获取第0个参数：用户传入的目标地址（因为此系统调用的发生就是用户传入一个结构体指针作为参数）

  // 从内核空间拷贝数据到用户空间
  // copyout是内核提供的安全复制函数，用于将数据从内核空间复制到用户空间
  if(copyout(myproc()->pagetable, dstaddr, (char*) &info, sizeof info) < 0)  // 参数列表：当前进程的页表（确保地址映射准确），。。。。。。
    return -1;
  return 0;
}
