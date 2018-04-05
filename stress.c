
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

#define STK_SZ 512
int
main(int argc, char *argv[])
{

  printf(1, "Starting Stress\n");

  printf(1, "Stack allocating %d bytes\n", STK_SZ);

  char data[STK_SZ];

  printf(1, "Stack allocated %d bytes\n", sizeof(data));

  printf(1, "Stress finished\n");

  exit();
}
