#include <stdio.h>
#include <string.h>
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>
#include <semaphore.h>

#include<helpers.h>
#include<fserver.h>

int sem_create(char *fname)
{

  // semaphore name needs a extra slash in front and _r/_w termination 
  char sem_fname_r[strlen(fname) + 3];
  char sem_fname_w[strlen(fname) + 3];
  sprintf(sem_fname_r, "/%s_r", fname);
  sprintf(sem_fname_w, "/%s_w", fname);
  if (DEBUG_LEVEL > 1) printf("Created semaphore for file: /%s\n",fname );

  // create new semaphore, return error if already there
  sem_t *r_semaddr = sem_open(sem_fname_r, O_CREAT|O_EXCL, S_IRWXU, 1);
  if ( r_semaddr == SEM_FAILED )
  {
    handle_error(-1, "Could not create read semaphore", NO_EXIT);
    return -1;
  }

  sem_t *w_semaddr = sem_open(sem_fname_w, O_CREAT|O_EXCL, S_IRWXU, 1);
  if ( w_semaddr == SEM_FAILED )
  {
    handle_error(-1, "Could not create write semaphore", NO_EXIT);
    sem_unlink(sem_fname_r);
    sem_close(r_semaddr);
    return -1;
  }

  return 0;
}

int sem_dec_r(char *fname)
{
  int retcode;
  // read semaphore name needs a extra slash in front and a _r at the end
  char sem_fname_r[strlen(fname) + 3];
  sprintf(sem_fname_r, "/%s_r", fname);

  // Get semaphore by name
  if (DEBUG_LEVEL > 1) printf("Decreasing read semaphore: %s\n",sem_fname_r );
  sem_t *r_sem_addr = sem_open(sem_fname_r, 0);

  // try to decrement
  retcode = sem_wait(r_sem_addr);
  if (retcode < 0 )
  {
    handle_error(retcode, "Couldnt decrement read semaphore", NO_EXIT);
    return -1;
  }

  return 0;
}

int sem_dec_w(char *fname)
{
  int retcode;
  // read semaphore name needs a extra slash in front and a _r at the end
  char sem_fname_w[strlen(fname) + 3];
  sprintf(sem_fname_w, "/%s_w", fname);

  if (DEBUG_LEVEL > 1) printf("Decreasing write semaphore: %s\n",sem_fname_w );

  // Get semaphore by name
  sem_t *w_sem_addr = sem_open(sem_fname_w, 0);

  // try to decrement
  retcode = sem_trywait(w_sem_addr);
  if (retcode < 0 )
  {
    handle_error(retcode, "Couldnt decrement write semaphore", NO_EXIT);
    return -1;
  }

  return 0;
}

int sem_inc_r (char* fname)
{
  int retcode;
  // read semaphore name needs a extra slash in front and a _r at the end
  char sem_fname_r[strlen(fname) + 3];
  sprintf(sem_fname_r, "/%s_r", fname);

  if (DEBUG_LEVEL > 1) printf("Increasing read semaphore: %s\n",sem_fname_r );

  // Get semaphore by name
  sem_t *r_sem_addr = sem_open(sem_fname_r, 0);

  retcode = sem_post(r_sem_addr);
  if (retcode < 0 )
  {
    handle_error(retcode, "Couldnt decrement read semaphore", NO_EXIT);
    return -1;
  }

  return 0;
}

int sem_inc_w (char* fname)
{
  int retcode;
  // read semaphore name needs a extra slash in front and a _w at the end
  char sem_fname_w[strlen(fname) + 3];
  sprintf(sem_fname_w, "/%s_w", fname);

  if (DEBUG_LEVEL > 1) printf("Increasing write semaphore: %s\n",sem_fname_w );

  // Get semaphore by name
  sem_t *w_sem_addr = sem_open(sem_fname_w, 0);

  // try to decrement
  retcode = sem_post(w_sem_addr);
  if (retcode < 0 )
  {
    handle_error(retcode, "Couldnt decrement read semaphore", NO_EXIT);
    return -1;
  }

  return 0;
}

int sem_get_r (char* fname)
{
  int retcode;
  // read semaphore name needs a extra slash in front and a _r at the end
  char sem_fname_r[strlen(fname) + 3];
  sprintf(sem_fname_r, "/%s_r", fname);

  if (DEBUG_LEVEL > 1) printf("Getting read semaphore value: %s\n",sem_fname_r );

  // Get semaphore by name
  sem_t *r_sem_addr = sem_open(sem_fname_r, 0);

  // get the value 
  int semval;
  retcode = sem_getvalue(r_sem_addr, &semval);
  if (retcode < 0 )
  {
    handle_error(retcode, "Couldnt get read semaphore value", NO_EXIT);
    return -1;
  }

  return semval;
}

int sem_get_w (char* fname)
{
  int retcode;
  // read semaphore name needs a extra slash in front and a _w at the end
  char sem_fname_w[strlen(fname) + 3];
  sprintf(sem_fname_w, "/%s_w", fname);

  if (DEBUG_LEVEL > 1) printf("Getting write semaphore value: %s\n",sem_fname_w );

  // Get semaphore by name
  sem_t *w_sem_addr = sem_open(sem_fname_w, 0);

  // get the value 
  int semval;
  retcode = sem_getvalue(w_sem_addr, &semval);
  if (retcode < 0 )
  {
    handle_error(retcode, "Couldnt get write semaphore value", NO_EXIT);
    return -1;
  }

  return semval;
}

int sem_kill (char* fname)
{
  int retcode;

  // semaphore name needs a extra slash in front and _r/_w termination 
  char sem_fname_r[strlen(fname) + 3];
  char sem_fname_w[strlen(fname) + 3];
  sprintf(sem_fname_r, "/%s_r", fname);
  sprintf(sem_fname_w, "/%s_w", fname);

  if (DEBUG_LEVEL > 1) printf("Kill semaphore for file: /%s\n",fname );

  // get semaphore
  sem_t *r_semaddr = sem_open(sem_fname_r, 0);
  if (r_semaddr == SEM_FAILED)
  {
    handle_error(-1, "sem_kill: Cannot get semaphore", NO_EXIT);
    return -1;
  }
  sem_t *w_semaddr = sem_open(sem_fname_w, 0);
  if (w_semaddr == SEM_FAILED)
  {
    handle_error(-1, "sem_kill: Cannot get semaphore", NO_EXIT);
    return -1;
  }

  retcode = sem_unlink(sem_fname_r);
  if (retcode < 0)
  {
    handle_error(retcode, "sem_kill: Cannot unlink semahore", NO_EXIT);
    return -1;
  }
  retcode = sem_unlink(sem_fname_w);
  if (retcode < 0)
  {
    handle_error(retcode, "sem_kill: Cannot unlink semahore", NO_EXIT);
    return -1;
  }

  retcode = sem_close(r_semaddr);
  if (retcode < 0)
  {
    handle_error(retcode, "sem_kill: Cannot close semahore", NO_EXIT);
    return -1;
  }
  retcode = sem_close(w_semaddr);
  if (retcode < 0)
  {
    handle_error(retcode, "sem_kill: Cannot close semahore", NO_EXIT);
    return -1;
  }

  return 0;
}
