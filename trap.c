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

pte_t * selectVictimPage(struct proc *p){

  pde_t *pde = &p->pgdir[0];
  pte_t *ptab = (pte_t*)PTE_ADDR(*pde);

  for(int i=4; i<NPTENTRIES; i++){
    //if(e < 2) e = 2;
    //int i = e;
    //if(ptab[i] == 0)
    //  continue;

    pte_t* pte = (void *)V2P(ptab+i);
    //pte_t* pte = (pte_t*)V2P((pte_t *)PTE_ADDR(*(pde+i)));

    if(*pte & PTE_P){
      //cprintf("Picked victim PTE: 0x%x, addr 0x%x\n", *pte, pte);


      //cprintf("Victim phys addr 0x%x\n", V2P(PTE_ADDR(ptab[i])));

      //void *victim = (void *)PTE_ADDR(ptab[i]);
      
      // Swap data to swap file

      //cprintf("0x%x Writing %d bytes to offset 0x%x\n", V2P(*pte),
      //  PGSIZE, PGSIZE * (p->nPages - p->nPhysPages));

      //writeToSwapFile(p, (void *)V2P(*pte), PGSIZE * (p->nPages - p->nPhysPages), PGSIZE);

      //cprintf("\n");

      //*pte &= ~PTE_P;
      //*pte |=  PTE_PG;


      //cprintf("New victim flags %x\n", *pte);

      //mappages(p->pgdir, (void *)PGROUNDDOWN(new), PGSIZE, (uint)(*pte), PTE_W|PTE_U);

      //*pte = PGSIZE * (p->nPages - p->nPhysPages) | PTE_FLAGS(*pte);


      //myproc()->nPages++;
      //cprintf("Mapped? %d\n", r);
      //cprintf("Mapped new memory from 0x%x to 0x%x\n", PGROUNDDOWN(new), victim);
      
      //break;
      return pte;
    }
  }

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

    //cprintf("\nPage fault! addr0x%x Process %d size %d eip0x%x\n",
    // rcr2(), myproc()->pid, myproc()->nPages, tf->eip);

    if(myproc()->nPages >= MAX_PAGES){
      cprintf("Using too much memory.  Killing...\n");
      myproc()->killed = 1;
    }
    else if(myproc()->nPages >= MAX_PHYS_PAGES){
      cprintf("----------------------\n");
      cprintf("Using more than physical memory. Swapping...\n");

      struct proc *p = myproc();

      uint new = rcr2();

      pte_t *pg = walkpgdir(p->pgdir, (void *)new, 0);

      cprintf("Page reference 0x%x\nNew page address 0x%x\n", new, pg);

      if(((int)*pg & PTE_PG)){
        cprintf("Swapped out\n");

        cprintf("Offset in file: 0x%x\n", PTE_ADDR(*pg));

        uint fileOffset = PTE_ADDR(*pg);

        char * buffer = kalloc();

        readFromSwapFile(p, buffer, fileOffset, PGSIZE);

        pte_t * victim = selectVictimPage(p);

        cprintf("Victim: 0x%x addr:0x%x\n", V2P(*victim), victim);

        writeToSwapFile(p, (void *)V2P(PTE_ADDR(*victim)), fileOffset, PGSIZE);
        
        *victim &= ~PTE_P;
        *victim |=  PTE_PG;

        memmove((char *)V2P(PTE_ADDR(*victim)), buffer, PGSIZE);

        cprintf("Faulted addr: 0x%x\n", new);

        mappages(p->pgdir, (void *)PGROUNDDOWN(new), PGSIZE, (uint)(PTE_ADDR(*victim)), PTE_W|PTE_U);

        *victim = fileOffset | PTE_FLAGS(*victim);

        //memmove()

        kfree(buffer);

        cprintf("Swapped page back in.\n");
        //p->killed = 1;
      } else {
        cprintf("Not swapped out\n");

        cprintf("Is present? %x\n", PTE_FLAGS(*pg));

        pte_t * pte = selectVictimPage(p);
        cprintf("Picked victim PTE: 0x%x, addr 0x%x\n", *pte, pte);

        // Swap data to swap file

        cprintf("0x%x Writing %d bytes to offset 0x%x\n", V2P(*pte),
          PGSIZE, PGSIZE * (p->nPages - p->nPhysPages));

        writeToSwapFile(p, (void *)V2P(PTE_ADDR(*pte)), PGSIZE * (p->nPages - p->nPhysPages), PGSIZE);

        *pte &= ~PTE_P;
        *pte |=  PTE_PG;

        cprintf("New victim flags %x\n", *pte);

        mappages(p->pgdir, (void *)PGROUNDDOWN(new), PGSIZE, (uint)PTE_ADDR(*pte), PTE_W|PTE_U);

        *pte = PGSIZE * (p->nPages - p->nPhysPages) | PTE_FLAGS(*pte);

      }

      cprintf("----------------------\n\n");
      p->nPages++;
      //myproc()->killed = 1;
    } else {

      //cprintf("Allocating memory\n");

      if(rcr2() > KERNBASE){
        cprintf("Memory address too high. Ignoring request\n");
        break;
      }
      //uint sz = myproc()->sz;

      uint a = PGROUNDDOWN(rcr2());

      struct proc *curproc = myproc();

      //cprintf("Physical Address: %x\n",V2P(rcr2()));

      char *mem = kalloc();

      if(mem == 0){
        cprintf("Could not allocate more memory. Killing...\n");
        curproc->killed = 1;
        break;
      }

      //cprintf("Newly allocated memory: va(0x%x) pa(0x%x)\n", mem, V2P(mem));

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
