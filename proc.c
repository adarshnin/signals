#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "signal.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct {
  struct spinlock lock;
  struct spinlock siglock;
} q;

static struct proc *initproc;

struct proc test;

// struct q {
//     void *ptr;
//     struct spinlock lock;
// };

void *p;
int paused = 0;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  int i;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  //clear pending signals
  for (i = 0; i < NSIG; i++){
    p->psignals[i] = 0; 
  }
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);


  //copy signal handlers from the parent
  for (i = 0; i < NSIG; i++){
    np->handlers[i] = curproc->handlers[i]; 
  }


  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

//int terminate_handler(struct proc *curproc){
//  curproc->state = ZOMBIE;
//  // Already killed? or to be killed??
//  curproc->killed = 1; 
//}
//



//
//int term_core_handler(struct proc *curproc){
//  curproc->state = RUNNABLE;
//  // Terminate the process and produce core dump
//}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

int
sendkill(int pid, int signum)
{
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      if(signum == SIGKILL){
        p->killed = 1;
      }
      else{
        
        p->psignals[signum] = 1; //For other signals
      }
      release(&ptable.lock);
      if (p->state == SLEEPING){
        if(paused == 1 && (signum == SIGTERM || signum == SIGINT || signum == SIGKILL)){
          // For processes which are SLEEPING by pause()
          paused = 0;
          handle_signal(p, SIGCONT);
          // Continue the process (to return -1) only when Terminate signals given
        }
        else if (paused == 0 && p->killed != 1){
          // For Stopped process
          handle_signal(p, signum);
        }
     
      }
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

int
pause()
{
  paused = 1;
  acquire(&q.lock);
  sleep(p, &q.lock);
  release(&q.lock);
  return -1;
}

int 
signal(int signum, sighandler_t handler)
{
  //can't set up uesr handler for SIGSTOP and SIGKILL 
  if (signum == SIGSTOP || signum == SIGKILL){
	  return -1;
  }
  struct proc *curproc = myproc();
  //some checking for SIGSTOP and SIGKILL is needed
  //
  acquire(&q.siglock);
  curproc->handlers[signum] = handler;
  release(&q.siglock);
  return 0;
}

//sigret function
int
sigret(void){
  struct proc *curproc = myproc();

  //restore the oldtrapframe
  memmove((void *)curproc->tf, (void *)curproc->oldtf, sizeof(struct trapframe));
  return 0;
}


//http://courses.cms.caltech.edu/cs124/lectures-wi2016/CS124Lec15.pdf
void
user_handler(struct proc *curproc, int i) 
{
  //user stack esp
  uint ustack_esp = curproc->tf->esp;

  //save the current trapframe from kernel stack to user stack
  ustack_esp -= sizeof(struct trapframe);
  memmove ((void*)ustack_esp, (void*)curproc->tf, sizeof(struct trapframe));
  curproc->oldtf = (struct trapframe *)ustack_esp;

  // Push the sigret_syscall.S code onto the user stack
   void *sig_ret_code_addr = (void *)execute_sigret_syscall_start;
   uint sig_ret_code_size = ((uint)&execute_sigret_syscall_end - (uint)&execute_sigret_syscall_start);
   
   // return addr for handler
   ustack_esp -= sig_ret_code_size;
   uint handler_ret_addr = ustack_esp;
   memmove((void *)ustack_esp, sig_ret_code_addr, sig_ret_code_size);

  //push the signal number
  ustack_esp -= sizeof(uint);
  *((uint *)ustack_esp) = i;

  //push the return address of sigret function
  ustack_esp -= sizeof(uint);
  memmove((void*)ustack_esp, (void*)&handler_ret_addr, sizeof(uint));
  //*((uint *)(ustack_esp)) = (uint)sigret;
  //curproc->tf->eax = 24;
  //curproc->tf->trapno = 64;
  //memmove((void*)ustack_esp, (void*)&trap, sizeof(uint));

  //change the esp stored in tf
  curproc->tf->esp = ustack_esp;

  //now change the eip to point to the user handler
  curproc->tf->eip = (uint)curproc->handlers[i];

  return; 
}




void 
handle_signal(struct proc *curproc, int i)
{
  if (curproc->handlers[i] == SIG_IGN)
    return;

  else if (curproc->handlers[i] == SIG_DFL){
    switch(i){
      case SIGSTOP:
      case SIGTSTP:
      case SIGTTIN:
      case SIGTTOU:
        stop_handler();
        break;
      case SIGCONT:
        cont_handler();
        break;
      case SIGTERM:
      case SIGINT:
      case SIGALRM:
      case SIGHUP:
      case SIGIO:
      case SIGPIPE:
      case SIGPROF:
      case SIGPWR:
      case SIGSTKFLT:
      case SIGUSR1:
      case SIGUSR2:
      case SIGVTALRM:
        term_handler(curproc);
        break;
      case SIGCHLD:
      case SIGURG:
      case SIGWINCH:
      // Doubt - ignore handler()
        break;
      default:
        break;
    }
  }

  else{
    user_handler(curproc, i);
    //curproc->tf->eip = (uint)curproc->handlers[i];
  }

  //clear the pending signal flag
  acquire(&q.siglock);
  curproc->psignals[i] = 0;
  release(&q.siglock);
  
}

void 
check_pending_signal(void) 
{
  struct proc *curproc = myproc();
  int i;

  //check pending signals
  for(i = 0; i < NSIG; i++){
    if (curproc->psignals[i])
      break;
  }
  if(i == NSIG)
    return;

  handle_signal(curproc, i);

}

void term_handler(struct proc *argp)
{
  struct proc *p = argp;
  acquire(&ptable.lock);
    p->killed = 1;
  if(p->state == SLEEPING){
    p->state = RUNNABLE;
  }
  // Terminate the process
  release(&ptable.lock);
}


void 
cont_handler()
{
  acquire(&q.lock);
  wakeup(p);
 // Continue the process
  release(&q.lock);
}

void 
stop_handler(){
  acquire(&q.lock);
  sleep(p, &q.lock);
  // Stop the process
  release(&q.lock);
}


//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
