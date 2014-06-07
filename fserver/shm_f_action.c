#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>

#include<helpers.h>
#include<fserver.h>

int create_shm_f(char *fname, char *fcontent)
{
  int fd;
  int retcode;

  // shared memory name needs a extra slash in front
  char shm_fname[strlen(fname) + 1];
  sprintf(shm_fname, "/%s", fname);
  if (DEBUG_LEVEL > 1) printf("Create a file: %s\n",shm_fname );
  //
  // size of content to save in shm
  int fcontent_size = strlen(fcontent)*sizeof(char);


  // create new shm segment, return error if already there
  fd = shm_open(shm_fname, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
  if(handle_error(fd, "Could not create shm", NO_EXIT) == -1)
  {
    return -1;
  }

  // resize freash shm to content size
  retcode = ftruncate(fd, fcontent_size);
  if(handle_error(retcode, "Could not truncate shm", NO_EXIT) == -1)
  {
    retcode = shm_unlink(shm_fname);
    handle_error(retcode, "Could not unlink shm", NO_EXIT);
    return -1;
  }

  // Map shared memory object
  char *shm_fcontent = mmap(NULL, fcontent_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(shm_fcontent == MAP_FAILED)
  {
    handle_error(-1, "mmap failed", NO_EXIT);
    retcode = shm_unlink(shm_fname);
    handle_error(retcode, "Could not unlink shm", NO_EXIT);
    return -1;
  }

  strcpy(shm_fcontent, fcontent);

  if (DEBUG_LEVEL > 1) printf("New shared memory file %s with the following content is born:\n%s\n", shm_fname, shm_fcontent);
  ///shm_unlink("/greeting");
  //printf("shm_unlinked:\n");
  return 0;
}

int update_shm_f(char *fname, char *fcontent)
{
  int fd;
  int retcode;

  // shared memory name needs a extra slash in front
  char shm_fname[strlen(fname) + 1];
  sprintf(shm_fname, "/%s", fname);
  if (DEBUG_LEVEL > 1) printf("Update file: %s\n",shm_fname );
  //
  // size of content to save in shm
  if (DEBUG_LEVEL > 1) printf("Before content size\n" );
  int fcontent_size = strlen(fcontent)*sizeof(char);


  // create new shm segment, return error if already there
  if (DEBUG_LEVEL > 1) printf("Before open\n" );
  fd = shm_open(shm_fname, O_RDWR, S_IRUSR | S_IWUSR);
  if (DEBUG_LEVEL > 1) printf("After open\n" );
  if(handle_error(fd, "Could not open shm", NO_EXIT) == -1)
  {
    return -1;
  }

  // resize shm to content size
  retcode = ftruncate(fd, fcontent_size);
  if(handle_error(retcode, "Could not truncate shm", NO_EXIT) == -1)
  {
    retcode = shm_unlink(shm_fname);
    handle_error(retcode, "Could not unlink shm", NO_EXIT);
    return -1;
  }

  // Map shared memory object
  char *shm_fcontent = mmap(NULL, fcontent_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(shm_fcontent == MAP_FAILED)
  {
    handle_error(-1, "mmap failed", NO_EXIT);
    retcode = shm_unlink(shm_fname);
    handle_error(retcode, "Could not unlink shm", NO_EXIT);
    return -1;
  }

  strcpy(shm_fcontent, fcontent);

  if (DEBUG_LEVEL > 1) printf("New shared memory file %s updated.\n", shm_fname);
  ///shm_unlink("/greeting");
  //printf("shm_unlinked:\n");
  return 0;
}

char *get_shm_f (char *fname)
{ // read a file from shared memory

  int retcode;
  // save shm info buffer:
  struct stat buf;

  // shared memory name needs a extra slash in front
  char shm_fname[strlen(fname) + 1];
  sprintf(shm_fname, "/%s", fname);
  if (DEBUG_LEVEL > 1) printf("Read file: %s\n",shm_fname );

  // get shm file descriptor by name
  int fd = shm_open(shm_fname, O_RDWR, 0);
  if(handle_error(fd, "Could not open shm", NO_EXIT) == -1)
  {
    return NULL;
  }

  // getting buffer cause I want to know the size
  retcode = fstat(fd, &buf);
  if(handle_error(retcode, "Could not get fstat of shm", NO_EXIT) == -1)
  {
    shm_unlink(shm_fname);
    return NULL;
  }

  if (DEBUG_LEVEL > 1) printf("fstat report size: %d\n", (int) buf.st_size);

  char *shm_fcontent = mmap(0, buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (shm_fcontent == MAP_FAILED)
  {
    handle_error(-1, "mmap failed.", NO_EXIT);
    shm_unlink(shm_fname);
    return NULL;
  }

  return shm_fcontent;
}

int delete_shm_f(char *fname)
{ // read a file from shared memory

  int retcode;

  // shared memory name needs a extra slash in front
  char shm_fname[strlen(fname) + 1];
  sprintf(shm_fname, "/%s", fname);
  if (DEBUG_LEVEL > 1) printf("Delete file: %s\n",shm_fname );

  // get shm file descriptor by name
  int fd = shm_open(shm_fname, O_RDWR, 0);
  if(handle_error(fd, "Could not open shm", NO_EXIT) == -1)
  {
    return -1;
  }

  retcode = shm_unlink(shm_fname);
  if(handle_error(retcode, "Deletion failed", NO_EXIT) == -1)
  {
    return -1;
  }

  return 0;
}


//int main (int argc, char **argv)
//{
//  char *fname = "cheerup";
//  char *fcontent = "FCB FCB FCB!";
//  char *fnewcontent = "Nananana Nananana Ehhh Ehhh Ehhh FCB!";
//  create_shm_f(fname, fcontent);
//  char *shm_content = get_shm_f(fname);
//  if(shm_content == NULL)
//  {
//    return -1;
//  }
//  printf("read shm content:\n%s\n\n", shm_content);
//
//  update_shm_f(fname, fnewcontent);
//  shm_content = get_shm_f(fname);
//  if(shm_content == NULL)
//  {
//    return -1;
//  }
//  printf("read new content:\n%s\n\n", shm_content);
//  delete_shm_f(fname);
//  return 0;
//}
