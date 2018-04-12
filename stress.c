
#include "types.h"
//#include "stat.h"
#include "user.h"
//#include "fs.h"
//#include "fcntl.h"

#define STK_SZ 512
#define MLC_SZ 4096*14
int
main(int argc, char *argv[])
{

  printf(1, "Starting Stress\n");

  //printf(1, "Stack allocating %d bytes\n", STK_SZ);

  //char data[STK_SZ];

  //printf(1, "Stack allocated  %d bytes\n", sizeof(data));


  printf(1, "\n\n\n");

  sleep(100);

  printf(1, "Malloc'ing %d bytes\n", MLC_SZ);

  int* mem = malloc(MLC_SZ);//(int *)sbrk(MLC_SZ/4096);//malloc(MLC_SZ);

  printf(1, "Malloc'd   %d bytes. 0x%x\n", MLC_SZ, mem);

  //memset(mem, 0, MLC_SZ);

  printf(1, "Zeroed     %d pages\n", MLC_SZ/4096);

  sleep(100);

  printf(1, "\nInserting numbers\n");
    
  int j;

  for(j=0; j<(MLC_SZ)/4; j++){
    mem[j] = j;
  }

  printf(1, "Finished Inserting numbers\n");

  sleep(100);

  //printf(1, "mem[0x%x]=%x\n", 4096*2/4, mem[4096*2/4]);

  int m = 0;
  int c = 0;
  int i;
  for(i=0; i<MLC_SZ/4; i++){
    if(mem[i] != i){
      printf(1, "Error with memory! %d: %d, %d\n", i, (mem[i]), c);
      m = i;
      c++;
    }
  }



  if(c > 0 || m > 0){
    printf(1, "Finished checking numbers. %d errors. Last error at 0x%x\n\n", &(mem[m]));
  } else {
    printf(1, "Finished checking numbers. No errors. last address: 0x%x\n\n", &(mem[i-1]));
  }
  sleep(10);
/*
  

  free(mem);//(int *)sbrk(-MLC_SZ/4096);//malloc(MLC_SZ);

  printf(1, "Unmalloc'd %d pages\n", MLC_SZ);

  sleep(100);

  printf(1, "Malloc'ing %d bytes\n", MLC_SZ);

  mem = malloc(MLC_SZ*2);//(int *)sbrk(MLC_SZ*2/4096);//malloc(MLC_SZ);

  printf(1, "Malloc'd   %d bytes. 0x%x\n", MLC_SZ*2, mem);

  //memset(mem, 0, MLC_SZ);

  printf(1, "Zeroed     %d pages\n", MLC_SZ*2/4096);

  sleep(100);

  printf(1, "\nInserting numbers\n");

  for(i=0; i<MLC_SZ*2/4; i++){
    mem[i] = i;

  }

  printf(1, "Finished Inserting numbers\n");

  for(i=0; i<MLC_SZ*2/4; i++){
    if(mem[i] != i){
      //printf(1, "Error with memory! %d: %d\n", i, (mem[i]));
      m = i;
      c++;
    }
  }

  if(c > 0 || m > 0){
    printf(1, "Finished checking numbers. %d errors. Last error at 0x%x\n\n", &(mem[m]));
  } else {
    printf(1, "Finished checking numbers. last address: 0x%x\n\n", &(mem[i-1]));
  }
  sleep(100);


  mem = (int *)sbrk(-MLC_SZ*2/4096);//malloc(MLC_SZ);*/
/*
  printf(1, "Malloc'ing %d bytes\n", MLC_SZ);

  char* mem2 = malloc(MLC_SZ);

  printf(1, "Malloc'd   %d bytes\n", MLC_SZ);

  memset(mem2, 0, MLC_SZ);

  printf(1, "Zeroed     %d pages\n", MLC_SZ/4096);

  sleep(100);*/

/*
  printf(1, "Malloc'ing %d bytes\n", MLC_SZ);

  char* mem3 = sbrk(1);//malloc(MLC_SZ);

  printf(1, "Malloc'd   %d bytes\n", MLC_SZ);

  memset(mem3, 0, MLC_SZ);

  printf(1, "Zeroed     %d pages\n", MLC_SZ/4096);

  sleep(100);*/


  sleep(10);

  free(mem);

  printf(1, "Stress finished\n");

  exit();
}
