
#include "types.h"
//#include "stat.h"
#include "user.h"
//#include "fs.h"
//#include "fcntl.h"

#define PGSIZE 4096
int
main(int argc, char *argv[])
{

	// args: ./m <pages> <request 1> <reqeust 2>...

	if(argc == 1){

	} else {

		int numPages = atoi(argv[1]); // converts string to an integer

		int * mem = (int *)sbrk(numPages);

		char * num = argv[2]-1;

		int i=0;
		for(i=0; num > 0; i++){
			

			//printf(1, "argv[2] loc: 0x%x\n", &argv[2]);
			printf(1, "First num loc: 0x%x\n", num); // prints the address for the first location

			printf(1, "num[%d]: %d\n", i, atoi((num+1)));
			int page = atoi(num+1);

			mem[(page-1)*PGSIZE/sizeof(int)] = i; //
			
			num = strchr(num+1, ',');
            sbrk(PGSIZE);
            printf(1, "Second num loc: 0x%x\n", num); // prints the address for the second location (causes a page fault)
            printf(1, "num[%d]: %d\n", i, atoi((num+1)));
		}

		sbrk(-numPages);

	}

	if(fork() != 0){
		wait();
        //sbrk(-16 * PGSIZE);
	} else {
		printf(1, "Child.\n");
		exit();
	}

	exit();
}
