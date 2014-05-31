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

const char *superv_name = "/leffotsirC";
// name for the file supervisor shm segment

int f_sv_dupl_check(char *fname)
{
  int fd;
  int fname_len = strlen(fname);
  struct file_supervisor *superv;

  printf("File supervisor checks if file %s already exists.\n", fname );

  // get shm segment with the file supervisor
  fd = shm_open(superv_name, O_RDWR, S_IRUSR | S_IWUSR);
  if(handle_error(fd, "f_sv_del: Could not open shm", NO_EXIT) == -1)
  {
    return -1;
  }
  //
  // Map shared memory object
  superv = mmap(NULL, sizeof(superv), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(superv == MAP_FAILED)
  {
    handle_error(-1, "f_sv_del: mmap failed", NO_EXIT);
    return -1;
  }

  int i = 0;
  while(1)
  { // loop and find file
    
    // check if we are inbound
    if( i >= F_LIMIT )
    {
      printf("f_sv_dupl_check: %s not found.\n", fname);
      return -1;
    }

    if(strncmp(superv->files[i], fname, fname_len) == 0 )
    { // we found it
      
      return 1;
      break;
    }
    i++;

  }
  return 0;
}

int f_sv_clean_shm()
{
  int retcode;
  // shared memory name needs a extra slash in front
  char shm_fname[strlen(superv_name) + 1];
  sprintf(shm_fname, "/%s", superv_name);
  printf("Delete shared memory for the file supervisor %s\n",shm_fname );

  retcode = shm_unlink(shm_fname);

  return handle_error(retcode, "Deletion of shm for file supervisor", NO_EXIT);

}

int f_sv_setup_shm()
{
  int fd;
  int retcode;
  struct file_supervisor *superv;

  // shared memory name needs a extra slash in front
  char shm_fname[strlen(superv_name) + 1];
  sprintf(shm_fname, "/%s", superv_name);
  printf("Create shared memory for the file supervisor %s\n",shm_fname );
  //
  // size of content to save in shm

  // create new shm segment, return error if already there
  fd = shm_open(shm_fname, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
  if(handle_error(fd, "Could not create shm", NO_EXIT) == -1)
  {
    return -1;
  }

  // resize freash shm to content size
  retcode = ftruncate(fd, sizeof(superv));
  if(handle_error(retcode, "Could not truncate shm", NO_EXIT) == -1)
  {
    retcode = shm_unlink(shm_fname);
    handle_error(retcode, "Could not unlink shm", NO_EXIT);
    return -1;
  }

  // Map shared memory object
  superv = mmap(NULL, sizeof(superv), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(superv == MAP_FAILED)
  {
    handle_error(-1, "mmap failed", NO_EXIT);
    return -1;
  }

  printf("Check\n");
  // Copy /END to the first place in the file name array
  strncpy(superv->files[0], "/END\0",5);

  printf("New shared memory %s for the file supervisor is born.\n", shm_fname);

  return 0;
}

int f_sv_add(char *fname)
{
  int fd;
  int fname_len = strlen(fname);
  struct file_supervisor *superv;

  // shared memory name needs a extra slash in front
  printf("File supervisor adds file: %s\n",fname );
  //

  // create new shm segment, return error if already there
  fd = shm_open(superv_name, O_RDWR, S_IRUSR | S_IWUSR);
  if(handle_error(fd, "Could not open shm", NO_EXIT) == -1)
  {
    return -1;
  }
  //
  // Map shared memory object
  superv = mmap(NULL, sizeof(superv), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(superv == MAP_FAILED)
  {
    handle_error(-1, "mmap failed", NO_EXIT);
    return -1;
  }

  int i = 0;
  while(1)
  { // loop through the files, find an empty place and cpy name
    
    // check if we are inbound
    if( i >= F_LIMIT )
    {
      printf("Memory full.\n");
      return -1;
    }


    if(strncmp(superv->files[i], "/END", 4) == 0 )
    { // this i the end of the file supervisir array 

      strncpy(superv->files[i], fname, fname_len < F_MAX_LEN ? fname_len : F_MAX_LEN);
      printf("Added new file to end of array, index is: %d\n", i);

      if ( (i+1) < F_LIMIT )
      { // there is free space for at least one more file name 

        strncpy(superv->files[i+1], "/END",4);
        superv->files[i+1][4] = '\00';
      }
      break;
    }

    else if (superv->files[i][0] == '\0')
    { // this place is free

      strncpy(superv->files[i], fname, fname_len < F_MAX_LEN ? fname_len : F_MAX_LEN);
      printf("Added new file to an empty gap at index is: %d\n", i);
      break;
    }
    else
    {
      i++;
    }
    // not reached
  }

  return 0;
}

int f_sv_del(char *fname)
{
  int fd;
  int fname_len = strlen(fname);
  struct file_supervisor *superv;

  // shared memory name needs a extra slash in front
  printf("File supervisor removes file: %s\n",fname );
  //

  // create new shm segment, return error if already there
  fd = shm_open(superv_name, O_RDWR, S_IRUSR | S_IWUSR);
  if(handle_error(fd, "f_sv_del: Could not open shm", NO_EXIT) == -1)
  {
    return -1;
  }
  //
  // Map shared memory object
  superv = mmap(NULL, sizeof(superv), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(superv == MAP_FAILED)
  {
    handle_error(-1, "f_sv_del: mmap failed", NO_EXIT);
    return -1;
  }

  int i = 0;
  while(1)
  { // loop through the files, find file and remove name
    
    // check if we are inbound
    if( i >= F_LIMIT )
    {
      printf("f_sv_del: %s not found.\n", fname);
      return -1;
    }

    if(strncmp(superv->files[i], fname, fname_len) == 0 )
    { // this is the file we have been looking for
      
      if (i+1 >= F_LIMIT)
      { // this is the last element of the array
          strncpy(superv->files[i], "/END",4);
          superv->files[i][4] = '\00';
          break;
      }

      // there is place for another file next to the current
      // we have to check if there are other entries after the current 
      if (strncmp(superv->files[i+1], "/END", 4) == 0)
      { // the next file place holder was the endmost used element in the array

        // now the current element is the endmost
        strncpy(superv->files[i], "/END",4);
        superv->files[i][4] = '\00';
      }
      else
      { // there are other file names after the current

        // we set the current file name to empty
        superv->files[i][0] = '\00';

      }

      break;
    }
    i++;

  }
  return 0;
}

struct file_supervisor *f_sv_getlist()
{
  int fd;
  struct file_supervisor *superv;

  // shared memory name needs a extra slash in front
  printf("Getting file supervisor: \n" );
  //

  // create new shm segment, return error if already there
  fd = shm_open(superv_name, O_RDWR, S_IRUSR | S_IWUSR);
  if(handle_error(fd, "Could not open shm for file supervisor", NO_EXIT) == -1)
  {
    return NULL;
  }
  //
  // Map shared memory object
  superv = mmap(NULL, sizeof(superv), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(superv == MAP_FAILED)
  {
    handle_error(-1, "mmap failed", NO_EXIT);
    return NULL;
  }

  return superv;
}
