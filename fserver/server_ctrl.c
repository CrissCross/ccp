#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <fserver.h>
#include <helpers.h>

#define MAX_CYCLE 20
int clean_up();
   
int main (int argc, char **argv)
{
  int retcode;

  if (DEBUG_LEVEL > 0) printf("\n Welcome to the TCP/SHM File Servrer!\n\n");
  // set up shared mem for the file supervisor
  retcode = f_sv_setup_shm();
  handle_my_error(retcode, "Couldnt set up shm for file supervisor", PROCESS_EXIT);

  int cyc_count = 0;
  while (1)
  { // break if max_cycle reached
    if(cyc_count >= MAX_CYCLE)
    { 
      break;
    }
    cyc_count++;
    if (DEBUG_LEVEL > 0) printf("\nCycle count: round %d\n", cyc_count);

    // get next command to handle
    struct cmd_info *cmd = get_cmd();
    if ( cmd == NULL )
    {
      if (DEBUG_LEVEL > 0) printf("Error while getting command.\n");
      if (DEBUG_LEVEL > 0) printf("Verify your command ...\n");
      continue;
    }
    if (DEBUG_LEVEL > 0) printf("Read cpmmand: enum = %d fname = %s content len = %d....\n", (int)cmd->cmd, cmd->fname, cmd->content_len);

    char *buf = NULL;
    switch ( cmd->cmd )
    { // LIST, CREATE, READ, UPDATE, DELETE, STOP
      case LIST:
        if (DEBUG_LEVEL > 0) printf("LIST\n");
        buf = prnt_ans(cmd, 0);
        break;

      case CREATE:
        if (DEBUG_LEVEL > 0) printf("CREATE\n");

        retcode = sem_create(cmd->fname);
        if (retcode < 0)
        {
          buf = prnt_ans(cmd, 0);
          break;
        }

        retcode = sem_dec_w(cmd->fname);
        if (retcode < 0)
        {
          buf = prnt_ans(cmd, 0);
          break;
        }

        // add entry to file superisor
        retcode = f_sv_add(cmd->fname);
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error CREATE: Adding file supervisor failed.", NO_EXIT);
          buf = prnt_ans(cmd, 0);
          sem_inc_w(cmd->fname);
          break;
        }

        // create shared memory
        retcode = create_shm_f(cmd->fname, "place holder");
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error CREATE: Couldnt create shared memory.", NO_EXIT);
          buf = prnt_ans(cmd, 0);
          sem_inc_w(cmd->fname);
          break;
        }

        // success!
        //
        sem_inc_w(cmd->fname);
        buf = prnt_ans(cmd, 1);
        break;

      case READ:
        if (DEBUG_LEVEL > 0) printf("READ\n");

        if ( f_sv_find_file(cmd->fname) < 0 )
        {
          buf = prnt_ans(cmd, 0);
          break;
        }

        // lock reader count semaphore (blocking) 
        sem_dec_r(cmd->fname);
        retcode = f_sv_addreader(cmd->fname);
        if ( retcode == 1 )
        { // we are the first reader, lets lock for writing
          retcode = sem_dec_w(cmd->fname);
          if ( retcode < 0 )
          { // cannot lock write semaphore. Obviously sb is busy on the file
            handle_my_error(-1, "READ: file is being changed, cannot read", NO_EXIT);
            buf = prnt_ans(cmd, 0);

            sem_inc_r(cmd->fname);
            f_sv_delreader(cmd->fname);
            sem_inc_r(cmd->fname);
            break;
          }
        }
        sem_inc_r(cmd->fname);

        buf = prnt_ans(cmd, 1);

        // finished reading
        sem_dec_r(cmd->fname);
        retcode = f_sv_delreader(cmd->fname);
        if ( retcode == 0 ) // we were the last reader
          retcode = sem_inc_w(cmd->fname);

        sem_inc_r(cmd->fname);

        break;

      case UPDATE:
        if (DEBUG_LEVEL > 0) printf("UPDATE\n");

        if ( f_sv_find_file(cmd->fname) < 0 )
        {
          buf = prnt_ans(cmd, 0);
          break;
        }

        // semaphore dekrementieren
        retcode = sem_dec_w(cmd->fname);
        if (retcode < 0)
        {
          sem_inc_w(cmd->fname);
          buf = prnt_ans(cmd, 0);
          break;
        }
        
        retcode = update_shm_f(cmd->fname, "Updated place holder");
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error UPDATE: Couldnt update shared memory.", NO_EXIT);
          sem_inc_w(cmd->fname);
          buf = prnt_ans(cmd, 0);
          break;
        }

        // success!
        sem_inc_w(cmd->fname);
        buf = prnt_ans(cmd, 1);
        break;

      case DELETE:
        if (DEBUG_LEVEL > 0) printf("DELETE\n");

        // semaphore dekrementieren
        retcode = sem_dec_w(cmd->fname);
        if (retcode < 0)
        {
          buf = prnt_ans(cmd, 0);
          break;
        }

        // delete entry on file supervisor
        retcode = f_sv_del(cmd->fname);
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error DELETE: Removing file supervisor failed.", NO_EXIT);
          buf = prnt_ans(cmd, 0);
          sem_inc_w(cmd->fname);
          break;
        }
        // delete shared mem
        retcode = delete_shm_f(cmd->fname);
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error DELETE: Couldnt delete shared memory.", NO_EXIT);
          buf = prnt_ans(cmd, 0);
          sem_inc_w(cmd->fname);
          break;
        }
        // delete semaphore
        sem_kill(cmd->fname);

        // success!
        buf = prnt_ans(cmd, 1);
        break;

      case STOP:
        if (DEBUG_LEVEL > 0) printf("STOP\n");
        cyc_count = MAX_CYCLE;
        break;

      default:
        if (DEBUG_LEVEL > 0) printf("DO not know.\n");
        break;
    }

    if ( buf != NULL )
    { // we have to print an answer and to free it afterwards
      // print answer to socket
      printf("%s\n", buf);
      // answer buffer and cmd info struct not needed anymore
      free(buf);
    }
    
    // clean cmd struct
    free(cmd->fname);
    //free(cinfo->content);
    free(cmd);
  }

  if (DEBUG_LEVEL > 0) printf("\nStopping server\n");
  clean_up();
  return 0;
}

int clean_up()
{
  struct file_supervisor *fsv = f_sv_getlist();

  if (DEBUG_LEVEL > 0) printf("Cleaning all shared memory\n");
  // clean all files from shared memory
  if ( fsv != NULL )
  {
    int i = 0;
    while (1)
    { //breaks if end of list reached

      if ( strncmp(fsv->files[i], "/END",4) == 0)
        break;

      else if ( fsv->files[i][0] !=  '\00' )
      {
        delete_shm_f(fsv->files[i]);
        sem_kill(fsv->files[i]);
        f_sv_del(fsv->files[i]);

      } 

      i++;
    }
  }

  // clean shared memory for file supervisor
  f_sv_clean_shm();
  return 0;
}
