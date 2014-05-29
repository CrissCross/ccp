#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>

int create_shm()
{
  int fd;
  char *msg = "Tscha Zaeaeaeme";
  int msg_size = strlen(msg)*sizeof(char);

  printf("SIze of msg is: %d\n",msg_size );

  fd = shm_open("/greeting", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (fd == -1)
    printf("Error\n");


  if (ftruncate(fd, msg_size) == -1)
    printf("Error\n");

  /* Map shared memory object */


  char *shm_msg = mmap(NULL, sizeof(msg),
             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  strcpy(shm_msg, msg);
  printf("Parent: thIS IS the shm_msg: %s\n", shm_msg);
  shm_unlink("/greeting");
  printf("shm_unlinked:\n");
  return 0;
}

int read_file ()
{ // read a file from shared memory
    struct stat buf;

    // get shm file descriptor by name
    int fd2 = shm_open("/greeting", O_RDWR, 0);

    // getting buffer cause I want to know the size
    fstat(fd2, &buf);
    printf("fstat report size: %d\n", (int) buf.st_size);

    char *shm_c_msg = mmap(0, buf.st_size, PROT_READ, MAP_SHARED, fd2, 0);
    if (shm_c_msg == MAP_FAILED)
    {
      printf("Error: %s\n",strerror(errno));
    }
    printf("shm_c_msg: %s\n", shm_c_msg);
    return 0;
}

int main (int argc, char **argv)
{
  create_shm();
}
