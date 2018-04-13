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

// These are defined in vm.c, and we want to use them.
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

// For debugging. Print the list in list-order
void listDump(struct proc *p){
  struct physPages cur = p->pds.list[p->pds.head];

  int i;
  for(i=0; i<MAX_PHYS_PAGES; i++){
    if(!cur.used)
      break;
    cprintf("(0x%x, %d), ", cur.va, cur.next);
    cur = p->pds.list[cur.next];
  }

  cprintf("\n");
}

// Find the item in the list that contains va, and remove it from the chain
void listRemove(struct proc *p, uint va){
  struct physPages *prev = &p->pds.list[p->pds.head];

  if(prev->va == va){ // edge case, head points to the target. move head to next

    p->pds.head = prev->next;
    prev->used = 0; // Free this array location

    return;
  }

  int i;
  int prevInd = p->pds.head;
  for(i=0; i<MAX_PHYS_PAGES-1; i++){
    if(p->pds.list[prev->next].va == va){ // next is target. Move pointers around.
      if(prev->next == p->pds.end){ // edge case, target is end. Move end to prev.
        p->pds.end = prevInd;

        p->pds.list[prev->next].used = 0;
      } else { // Normal case, point the previous to the next
        struct physPages *next = &p->pds.list[prev->next];

        prev->next = next->next;
        next->used = 0;
      }
      //cprintf("Rmv: ");
      //listDump(p);
      return;
    }

    prevInd = prev->next;
    prev = &p->pds.list[prev->next];
  }
}

// Returns whether the list contains an element with va
int listContains(struct proc *p, uint va){

  int i=0;
  for(i=0; i<MAX_PHYS_PAGES; i++){
    if(p->pds.list[i].used && p->pds.list[i].va == va)
      return 1;
  }

  return 0;
}

// Add an element to the end of the list
void listAdd(struct proc *p, uint va){

  // If the element is in the list, remove it first (avoid filling the array with the same value)
  if(listContains(p, va)){
    listRemove(p, va);
  }

  if(p->pds.list[p->pds.head].used == 0){ // If there is nothing in the list (edge case)
    p->pds.end = 0;
    p->pds.list[0].va = va;
    p->pds.list[0].next = 1;
    p->pds.list[0].used = 1;
  } else { 

    struct physPages *last = &p->pds.list[p->pds.end];

    // Search for the next available slot in the array, and add that to the list
    struct physPages *avail = &p->pds.list[p->pds.head];
    int i;
    for(i=0; i<MAX_PHYS_PAGES; i++){
      if(p->pds.list[i].used == 0){
        avail = &p->pds.list[i];
        break;
      }
    }

    avail->va = va;
    avail->used = 1;

    last->next = i;

    p->pds.end = i;
  }

  //cprintf("Add: ");
  //listDump(p);
}

// Remove the element that is pointed to by the head (queue-equivalent)
uint listPop(struct proc *p){
  uint pop = p->pds.list[p->pds.head].va;

  p->pds.list[p->pds.head].used = 0;
  p->pds.head = p->pds.list[p->pds.head].next;

  //cprintf("Pop: ");
  //listDump(p);
  return pop;
}

// Generate random number with LCG using GCC parameters
uint randint(){
  static int a = 1103515245;
  static int c = 12345;
  static int m = 1 << 31;
  static int seed = 443300;

  seed = (a * seed + c) % m;

  return seed;
}

// Pick a victim by rule
pte_t * selectVictimPage(struct proc *p){

#ifdef FIFO

  //return listPop(p);
  uint va = listPop(p); // The LRU structure happens to work well for FIFO!
  pte_t *pte = walkpgdir(p->pgdir, (void *)va, 0); // Convert the va to pte

  return pte;

#elif RAND // Paul

  //pde_t *pde = &p->pgdir[0];
  //pte_t *ptab = (pte_t*)PTE_ADDR(*pde);

  //int *i = &p->fifoPointer;

  uint rnd = randint(); // Pick a random number

  rnd = rnd % p->nPhysPages; // Squash random number to the range of selectable entries

  //pte_t* pte = 0;

  int i = 0;
  // Loop through the page table until we've seen rnd present pages, then choose that one
  /*for(i=0; rnd > 0; i = (i+1)%NPTENTRIES){

    pte = (void *)V2P(ptab+i);

    if(*pte & PTE_P){
      rnd--;
    }
  }*/

  uint a = listPop(p);
  for(i=0; i<rnd; i++){
    listAdd(p, a);
    a = listPop(p);
  }

  pte_t *pte = walkpgdir(p->pgdir, (void *)a, 0); // Convert the va to pte

  return pte;

#elif LRU // Default: LRU

  uint va = listPop(p); // Pop from the physical pages list 
  pte_t *pte = walkpgdir(p->pgdir, (void *)va, 0); // convert va to pte

  return pte;

#endif

}

// Update the page data structure (Only for LRU)
void updateLRU(struct proc *p){
  // The LRU ds is actually a queue (FIFO), where when an element is used, it goes to 
  // the back of the line.  Imagine a bank where the customers must deposit all of their
  // earnings at once. Now imagine the customers are making money in realtime, by let's say
  // selling something on the street.  When they make a sale, they must leave the bank
  // line, make the sale, and then go to the back of the line. The bank customer who does
  // not leave the line as frequently will end up towards the front of the line, until they
  // are serviced.

  pte_t *pte;
  int i;
  for(i=0; i<p->nPhysPages; i++){
    pte = walkpgdir(p->pgdir, (void *)p->pds.list[i].va, 0);
    if((*pte & PTE_A) && (*pte & PTE_P)){
      if(listContains(p, (uint)p->pds.list[i].va)){
        listRemove(p, (uint)p->pds.list[i].va);
        listAdd(p, (uint)p->pds.list[i].va);
      }
    }
    *pte &= ~PTE_A;
  }
  /*for(i=0; i<p->nPages; i+=PGSIZE){
    pte = walkpgdir(p->pgdir, (char *)i, 0);
    if((*pte & PTE_A) && (*pte & PTE_P)){ //check PTE_A
      if(listContains(p, (uint)i)){
        cprintf("Pid %d Contains 0x%x\n", p->pid, i);
        //listRemove(p, (uint)i); // Remove it from the queue
        //listAdd(p, (uint)i); // Add it back to the queue
      }
    }
    *pte &= ~PTE_A; //reset PTE_A
  }*/
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

    if(myproc()->nPages >= MAX_PAGES){ // Memory usage beyond limits, kill the process
      cprintf("Using too much memory.  Killing...\n");
      myproc()->killed = 1;
    }
    else if(myproc()->nPages >= MAX_PHYS_PAGES){ // Must swap a page out

      struct proc *p = myproc();

      //updateLRU(p);

      uint new = rcr2();

      pte_t *pg = walkpgdir(p->pgdir, (void *)new, 0);


      if(((int)*pg & PTE_PG)){ // Faulted page was swapped out

        //cprintf("Page reference 0x%x\n", new);

        uint fileOffset = PTE_ADDR(*pg);

        char * buffer = kalloc();

        readFromSwapFile(p, buffer, fileOffset, PGSIZE);

        pte_t * victim = selectVictimPage(p);

        //cprintf("Swapped out victim: 0x%x\n", victim);

        writeToSwapFile(p, (void *)V2P(PTE_ADDR(*victim)), fileOffset, PGSIZE);
        
        *victim &= ~PTE_P;
        *victim |=  PTE_PG;

        memmove((char *)V2P(PTE_ADDR(*victim)), buffer, PGSIZE);

        mappages(p->pgdir, (void *)PGROUNDDOWN(new), PGSIZE, (uint)(PTE_ADDR(*victim)), PTE_W|PTE_U);

        *victim = fileOffset | PTE_FLAGS(*victim);

        lcr3(V2P(p->pgdir));  // switch to process's address space

        kfree(buffer);

      } else {  // Faulted page does not yet exist
        pte_t * pte = selectVictimPage(p);
        //cprintf("Not swapped out victim: 0x%x\n", pte);

        writeToSwapFile(p, (void *)V2P(PTE_ADDR(*pte)), PGSIZE * (p->nPages - p->nPhysPages), PGSIZE);

        *pte &= ~PTE_P;
        *pte |=  PTE_PG;

        mappages(p->pgdir, (void *)PGROUNDDOWN(new), PGSIZE, (uint)PTE_ADDR(*pte), PTE_W|PTE_U);

        *pte = PGSIZE * (p->nPages - p->nPhysPages) | PTE_FLAGS(*pte);

        memset((void *)V2P(PTE_ADDR(*pte)), 0, PGSIZE);

        lcr3(V2P(p->pgdir));  // switch to process's address space

        p->nPages++;

      }

      listAdd(p, PGROUNDDOWN(new));
    } else { // Have not run out of physical memory yet

      uint a = PGROUNDDOWN(rcr2());

      struct proc *curproc = myproc();

      char *mem = kalloc();

      if(mem == 0){
        cprintf("Could not allocate more memory. Killing...\n");
        curproc->killed = 1;
        break;
      }
      memset(mem, 0, PGSIZE);

      listAdd(curproc, a);

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
     tf->trapno == T_IRQ0+IRQ_TIMER){

      // update the LRU structure only if we are using LRU!
      #if  defined(LRU)

      struct proc *ps;
      getproc(&ps);

      struct proc *p;
      // Find the current running process, and update its LRU structure
      for(p = ps;  p < &ps[NPROC]; p++){
        if(p->state == RUNNING){
          updateLRU(p);
          break;
        }
      }
      //popcli();
      #endif

    yield();
  }

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
