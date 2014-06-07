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

const char *superv_name = "leffotsirC";
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

  // Copy /END to the first place in the file name array
  strncpy(superv->files[0], "/END",4);
  superv->files[0][4] = '\00';

  // set count to 0
  superv->count = 0;

  printf("Created new shared memory segment %s for the file supervisor.\n\n", shm_fname);

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
    return handle_error(-1, "mmap failed", NO_EXIT);

  int i = 0;
  while(1)
  { // loop through the files, find an empty place and cpy name
    
    // check if we are inbound
    if( i >= F_LIMIT )
    {
      printf("Memory full.\n");
      return -1;
    }


    int endTest = strncmp(superv->files[i], "/END", 4);
    if(endTest == 0 || superv->files[i][0] == '\00')
    { // this place is free 

      // copy file name and increase counter
      strncpy(superv->files[i], fname, (fname_len+1) < F_MAX_LEN ? (fname_len+1) : F_MAX_LEN);
      superv->files[i][fname_len] = '\00';
      superv->count++;

      printf("Added new file, index is: %d, we have now %d  files on the server.\n", i, superv->count);

      if ( (i+1) < F_LIMIT )
      { // there is free space for at least one more file name, lets mark the end

        strncpy(superv->files[i+1], "/END",4);
        superv->files[i+1][4] = '\00';
      }

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
  char errmsg[100];

  // shared memory name needs a extra slash in front
  printf("File supervisor removes file: %s\n",fname );
  //

  // create new shm segment, return error if already there
  fd = shm_open(superv_name, O_RDWR, S_IRUSR | S_IWUSR);
  if( fd < 0 )
  {
    snprintf(errmsg, 100, "f_sv_del: Could not open shm for file %s.", fname);
    handle_error(fd, errmsg, NO_EXIT);
    return -1;
  }
  //
  // Map shared memory object
  superv = mmap(NULL, sizeof(superv), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(superv == MAP_FAILED)
  {
    snprintf(errmsg, 100, "f_sv_del: mmap for file %s failed.", fname);
    handle_ptr_error(superv, errmsg, NO_EXIT);
    return -1;
  }

  int i = 0;
  while(1)
  { // loop through the files, find file and remove name
    
    // check if we are inbound
    if( i >= F_LIMIT )
    {
      snprintf(errmsg, 100, "f_sv_del: file %s not found.", fname);
      handle_my_error(-1, errmsg, NO_EXIT);
      return -1;
    }

    if(strncmp(superv->files[i], fname, fname_len) == 0 )
    { // this is the file we have been looking for
      
      // decrease number of files count
      superv->count--;

      if (i - superv->count >= 0)
      { // I am the last file in the array, lets mark the end 
          strncpy(superv->files[i], "/END",4);
          superv->files[i][4] = '\00';
      }
      else
      { // there are other file names after the current
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
  if(handle_error(fd, "f_sv_getlist(): Could not open shm for file supervisor", NO_EXIT) == -1)
  {
    return NULL;
  }
  //
  // Map shared memory object
  superv = mmap(NULL, sizeof(superv), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(superv == MAP_FAILED)
  {
    handle_ptr_error(superv, "f_sv_getlist(): mmap failed", NO_EXIT);
    return NULL;
  }

  return superv;
}
