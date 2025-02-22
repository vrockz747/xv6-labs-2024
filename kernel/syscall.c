#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz) // both tests needed, in case of overflow
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_trace(void);

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_trace]   sys_trace,
};

void print_trace(int sys_num, int mask, int pid, int ret) {
    switch (sys_num) {
        case SYS_fork:   if ((1 << SYS_fork) & mask)   printf("%d: syscall fork -> %d\n", pid, ret); break;
        case SYS_exit:   if ((1 << SYS_exit) & mask)   printf("%d: syscall exit -> %d\n", pid, ret); break;
        case SYS_wait:   if ((1 << SYS_wait) & mask)   printf("%d: syscall wait -> %d\n", pid, ret); break;
        case SYS_pipe:   if ((1 << SYS_pipe) & mask)   printf("%d: syscall pipe -> %d\n", pid, ret); break;
        case SYS_read:   if ((1 << SYS_read) & mask)   printf("%d: syscall read -> %d\n", pid, ret); break;
        case SYS_kill:   if ((1 << SYS_kill) & mask)   printf("%d: syscall kill -> %d\n", pid, ret); break;
        case SYS_exec:   if ((1 << SYS_exec) & mask)   printf("%d: syscall exec -> %d\n", pid, ret); break;
        case SYS_fstat:  if ((1 << SYS_fstat) & mask)  printf("%d: syscall fstat -> %d\n", pid, ret); break;
        case SYS_chdir:  if ((1 << SYS_chdir) & mask)  printf("%d: syscall chdir -> %d\n", pid, ret); break;
        case SYS_dup:    if ((1 << SYS_dup) & mask)    printf("%d: syscall dup -> %d\n", pid, ret); break;
        case SYS_getpid: if ((1 << SYS_getpid) & mask) printf("%d: syscall getpid -> %d\n", pid, ret); break;
        case SYS_sbrk:   if ((1 << SYS_sbrk) & mask)   printf("%d: syscall sbrk -> %d\n", pid, ret); break;
        case SYS_sleep:  if ((1 << SYS_sleep) & mask)  printf("%d: syscall sleep -> %d\n", pid, ret); break;
        case SYS_uptime: if ((1 << SYS_uptime) & mask) printf("%d: syscall uptime -> %d\n", pid, ret); break;
        case SYS_open:   if ((1 << SYS_open) & mask)   printf("%d: syscall open -> %d\n", pid, ret); break;
        case SYS_write:  if ((1 << SYS_write) & mask)  printf("%d: syscall write -> %d\n", pid, ret); break;
        case SYS_mknod:  if ((1 << SYS_mknod) & mask)  printf("%d: syscall mknod -> %d\n", pid, ret); break;
        case SYS_unlink: if ((1 << SYS_unlink) & mask) printf("%d: syscall unlink -> %d\n", pid, ret); break;
        case SYS_link:   if ((1 << SYS_link) & mask)   printf("%d: syscall link -> %d\n", pid, ret); break;
        case SYS_mkdir:  if ((1 << SYS_mkdir) & mask)  printf("%d: syscall mkdir -> %d\n", pid, ret); break;
        case SYS_close:  if ((1 << SYS_close) & mask)  printf("%d: syscall close -> %d\n", pid, ret); break;
        case SYS_trace:  if ((1 << SYS_trace) & mask)  printf("%d: syscall trace -> %d\n", pid, ret); break;
        default: printf("Error: Unkown syscall\n"); break;
    }
}

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
    print_trace(num, p->trace_mask,p->pid, p->trapframe->a0);
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
