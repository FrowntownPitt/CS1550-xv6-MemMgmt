
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

	char args[] = {14,10,10,12,13,15,16,19,14,17,18,12,20,18,20,19,16,12,11,19,16,20,14,14,13,19,19,20,12,17,14,20,18,10,19,14,11,12,15,20,15,11,18,18,20,12,16,11,11,15,18,17,14,10,14,19,17,11,11,16,12,11,14,13,17,11,17,12,15,14,15,20,13,14,11,11,14,17,15,13,15,10,10,17,17,18,20,15,19,20,11,19,14,16,14,15,19,15,20,10};


	int numPages = 0;
	if(argc == 1){
		numPages = 1;
	} else {
		numPages = atoi(argv[1]);
	}

	int * mem = (int *)sbrk(numPages);

		//char * num = argv[2]-1;

	int i=0;
	for(i=0; i<sizeof(args)/sizeof(args[0]); i++){
		

		//printf(1, "argv[2] loc: 0x%x\n", &argv[2]);
		//printf(1, "First num loc: 0x%x\n", num);

		printf(1, "num[%d]: %d\n", i, args[i]);
		int page = args[i];//atoi(num+1);

		mem[(page-1)*PGSIZE/sizeof(int)] = i;
		
		//num = strchr(num+1, ',');
	}

	sbrk(-numPages);


	/*if(fork() != 0){
		wait();
	} else {
		printf(1, "Child.\n");
		exit();
	}*/

	exit();
}