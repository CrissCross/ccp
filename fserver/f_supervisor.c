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

// unique name for the file supervisor shm segment
const char *superv_name = "leffotsirC";

int f_sv_clean_shm()
{
  int retcode;
  // shared memory name needs a extra slash in front
  char shm_fname[strlen(superv_name) + 1];
  sprintf(shm_fname, "/%s", superv_name);
  if (DEBUG_LEVEL > 1) printf("Delete shared memory for the file supervisor %s\n",shm_fname );

  retcode = shm_unlink(shm_fname);

  return handle_error(retcode, "Deletion of shm for file supervisor", NO_EXIT);

}

int get_index (char *fname, struct file_supervisor *superv)
{ // find the file name in the list and return the index

  int f_len = strlen(fname);
  char errmsg[100];

  int i = 0;
  while(1)
  { // loop through the files, find file and remove name
    
    int index_f_len = strlen(superv->files[i]);
    int endTest = strncmp(superv->files[i], "/END", 4);
    
    // check if we are inbound
    if( i >= F_LIMIT || endTest == 0)
    {
      snprintf(errmsg, 100, "get_index: file %s not found.", fname);
      handle_my_error(-1, errmsg, NO_EXIT);
      return -1;
    }

    if( f_len == index_f_len && strncmp(superv->files[i], fname, f_len) == 0 )
    { // this is the file we have been looking for
      return i;
    }

    i++;
  }
  
  // not reached
  return -1;
}

struct file_supervisor *f_sv_getlist()
{
  int fd;
  struct file_supervisor *superv;

  // shared memory name needs a extra slash in front
  if (DEBUG_LEVEL > 1) printf("Getting file supervisor. \n" );
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

  if (DEBUG_LEVEL > 1) printf("Created new shared memory segment %s for the file supervisor.\n\n", shm_fname);

  return 0;
}

int f_sv_add(char *fname)
{
  if (DEBUG_LEVEL > 1) printf("File supervisor adds file: %s\n",fname );

  int fname_len = strlen(fname);
  struct file_supervisor *superv = f_sv_getlist();
  if ( superv == NULL )
    return -1;

  int i = 0;
  while(1)
  { // loop through the files, find an empty place and cpy name
    
    // check if we are inbound
    if( i >= F_LIMIT )
    {
      handle_my_error(-1, "f_fs_add: Memory full.", NO_EXIT);
      return -1;
    }


    int endTest = strncmp(superv->files[i], "/END", 4);
    if(endTest == 0 || superv->files[i][0] == '\00')
    { // this place is free 

      // copy file name and increase counter
      strncpy(superv->files[i], fname, (fname_len+1) < F_MAX_LEN ? (fname_len+1) : F_MAX_LEN);
      superv->files[i][fname_len] = '\00';
      superv->reader_count[i] = 0;
      superv->count++;

      if (DEBUG_LEVEL > 1) printf("Added new file, index is: %d, we have now %d  files on the server.\n", i, superv->count);

      if ( endTest == 0 && (i+1) < F_LIMIT )
      { // We are at the end of the list and there is free space for at least one more file name.
        // Lets mark the new end:
        strncpy(superv->files[i+1], "/END",4);
        superv->files[i+1][4] = '\00';
      }

      break;
    }
    else
    { // this index is already taken by another file
      i++;
    }
    // not reached
  }

  return 0;
}

int f_sv_del(char *fname)
{
  // shared memory name needs a extra slash in front
  if (DEBUG_LEVEL > 1) printf("File supervisor removes file: %s\n",fname );

  // getting supervisor struct
  struct file_supervisor *superv = f_sv_getlist();
  if ( superv == NULL )
    return -1;

  // getting index of the file in the struct
  int index = get_index(fname, superv);
  if ( index < 0 )
    return index;

  if(DEBUG_LEVEL > 1) printf("name = %s, index = %d\n", fname, index);

  // if the next file is '\END', this file is the new end 
  int endTest = strncmp(superv->files[index+1], "/END", 4);
  if (endTest == 0)
  { // ...it is the last file in the array, lets mark the end 
    strncpy(superv->files[index], "/END",4);
    superv->files[index][4] = '\00';
  }
  else
  { // there are other file names after the current
    superv->files[index][0] = '\00';
  }
  superv->count--;

  return 0;
}

int f_sv_addreader(char *fname)
{

  // getting supervisor struct from shared memory
  struct file_supervisor *superv = f_sv_getlist();
  if ( superv == NULL )
    return -1;


  // getting index of the file 
  int index = get_index(fname, superv);
  if ( index < 0 )
    return index;


  // increment on readers count
  superv->reader_count[index]++;

  return superv->reader_count[index];
}

int f_sv_delreader(char *fname)
{

  // getting supervisor struct from shared memory
  struct file_supervisor *superv = f_sv_getlist();

  // getting index of the file 
  int index = get_index(fname, superv);

  // increment on readers count
  superv->reader_count[index]--;

  return superv->reader_count[index];
}

int f_sv_find_file (char *fname)
{ // returns -1 if not found
  struct file_supervisor *fsv = f_sv_getlist();
  return get_index(fname, fsv);
}
