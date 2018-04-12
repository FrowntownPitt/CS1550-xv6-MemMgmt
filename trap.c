#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

pte_t *walkpgdir(pde_t *pgdir, const void *va, int alloc);
int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

void addToStack(struct proc *p, struct physPages pg){



  return;
}

struct physPages popFromStack(struct proc *p){



  return p->stack[0];
}

uint randint(){
  static int a = 1103515245;
  static int c = 12345;
  static int m = 1 << 31;
  static int seed = 443300;

  seed = (a * seed + c) % m;

  //seed = x;

  return seed;

}

pte_t * selectVictimPage(struct proc *p){

  pde_t *pde = &p->pgdir[0];
  pte_t *ptab = (pte_t*)PTE_ADDR(*pde);

#ifdef FIFO

  int *i = &p->fifoPointer;

  for(; *i<NPTENTRIES; *i = (*i+1)%NPTENTRIES){

    pte_t* pte = (void *)V2P(ptab+*i);

    if(*pte & PTE_P){
      return pte;
    }
  }

#elif RAND

  int *i = &p->fifoPointer;

  uint rnd = randint();

  rnd %= p->nPages;

  pte_t* pte = 0;

  for(; rnd > 0; *i = (*i+1)%NPTENTRIES){

    pte = (void *)V2P(ptab+*i);

    if(*pte & PTE_P){
      rnd--;
    }
  }

  return pte;

#else // Default: LRU
#endif

  return 0;
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:

    myproc()->pageFaults++;

    if(myproc()->nPages >= MAX_PAGES){
      cprintf("Using too much memory.  Killing...\n");
      myproc()->killed = 1;
    }
    else if(myproc()->nPages >= MAX_PHYS_PAGES){

      struct proc *p = myproc();

      uint new = rcr2();

      pte_t *pg = walkpgdir(p->pgdir, (void *)new, 0);


      if(((int)*pg & PTE_PG)){

        cprintf("Page reference 0x%x\n", new);

        uint fileOffset = PTE_ADDR(*pg);

        char * buffer = kalloc();

        readFromSwapFile(p, buffer, fileOffset, PGSIZE);

        pte_t * victim = selectVictimPage(p);

        writeToSwapFile(p, (void *)V2P(PTE_ADDR(*victim)), fileOffset, PGSIZE);
        
        *victim &= ~PTE_P;
        *victim |=  PTE_PG;

        memmove((char *)V2P(PTE_ADDR(*victim)), buffer, PGSIZE);

        mappages(p->pgdir, (void *)PGROUNDDOWN(new), PGSIZE, (uint)(PTE_ADDR(*victim)), PTE_W|PTE_U);

        *victim = fileOffset | PTE_FLAGS(*victim);

        kfree(buffer);
      } else {
        pte_t * pte = selectVictimPage(p);

        writeToSwapFile(p, (void *)V2P(PTE_ADDR(*pte)), PGSIZE * (p->nPages - p->nPhysPages), PGSIZE);

        *pte &= ~PTE_P;
        *pte |=  PTE_PG;

        mappages(p->pgdir, (void *)PGROUNDDOWN(new), PGSIZE, (uint)PTE_ADDR(*pte), PTE_W|PTE_U);

        *pte = PGSIZE * (p->nPages - p->nPhysPages) | PTE_FLAGS(*pte);

        memset((void *)V2P(PTE_ADDR(*pte)), 0, PGSIZE);

        p->nPages++;

      }
    } else {

      uint a = PGROUNDDOWN(rcr2());

      struct proc *curproc = myproc();

      char *mem = kalloc();

      if(mem == 0){
        cprintf("Could not allocate more memory. Killing...\n");
        curproc->killed = 1;
        break;
      }
      memset(mem, 0, PGSIZE);

      if(mappages(curproc->pgdir, (char *)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
        cprintf("Could not allocate more memory. Killing...\n");
        curproc->killed = 1;
        break;
      }

      myproc()->nPhysPages++;
      myproc()->nPages++;
    }
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER){
    //cprintf("Exiting\n");
    exit();
  }

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
