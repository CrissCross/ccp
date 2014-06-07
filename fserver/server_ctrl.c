#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <fserver.h>
#include <helpers.h>

#define MAX_CYCLE 10
int clean_up();
int print_curr_files()
{
  struct file_supervisor *superv = f_sv_getlist();
  if (superv->count == 0)
  {
    printf("There are no files at the moment.\n");
    return 0;
  }
  
  printf("We have %d files:\n", superv->count);
  int i = 0;
  while (1)
  { //breaks if end of list reached 

    if (strncmp(superv->files[i], "/END", 4) == 0)
      break;

    if ( superv->files[i][0] != '\00' )
    {
      printf("%d \t %s\n", i, superv->files[i]);
    }
    i++;

  }
  printf("\n");
  return 0;
}
  
int main (int argc, char **argv)
{
  int retcode;

  printf("\n Welcome to the TCP/SHM File Servrer!\n\n");
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
    printf("\nCycle count: round %d\n", cyc_count);

    struct cmd_info *cmd = get_cmd();
    if ( cmd == NULL )
    {
      printf("Error while getting command.\n");
      printf("Review the command file and try again...\n");
      break;
    }

    printf("Read cpmmand: enum = %d fname = %s content len = %d....\n", (int)cmd->cmd, cmd->fname, cmd->content_len);

    char *buf = NULL;
    switch ( cmd->cmd )
    { // LIST, CREATE, READ, UPDATE, DELETE, STOP
      case LIST:
        printf("LIST\n");
        buf = prnt_ans(cmd, 0);
        break;

      case CREATE:
        printf("CREATE\n");

        // add entry to file superisor
        retcode = f_sv_add(cmd->fname);
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error CREATE: Adding file supervisor failed.", NO_EXIT);
          buf = prnt_ans(cmd, 0);
          break;
        }

        // create shared memory
        retcode = create_shm_f(cmd->fname, "place holder");
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error CREATE: Couldnt create shared memory.", NO_EXIT);
          buf = prnt_ans(cmd, 0);
          break;
        }

        // success!
        buf = prnt_ans(cmd, 1);
        break;

      case READ:
        printf("READ\n");
        buf = prnt_ans(cmd, 1);
        break;

      case UPDATE:
        printf("UPDATE\n");

        retcode = update_shm_f(cmd->fname, "Updated place holder");
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error UPDATE: Couldnt update shared memory.", NO_EXIT);
          buf = prnt_ans(cmd, 0);
          break;
        }

        // success!
        buf = prnt_ans(cmd, 1);
        break;

      case DELETE:
        printf("DELETE\n");

        // delete entry on file supervisor
        retcode = f_sv_del(cmd->fname);
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error DELETE: Removing file supervisor failed.", NO_EXIT);
          buf = prnt_ans(cmd, 0);
          break;
        }

        // delete shared mem
        retcode = delete_shm_f(cmd->fname);
        if ( retcode < 0 )
        {
          handle_my_error(retcode, "Error DELETE: Couldnt delete shared memory.", NO_EXIT);
          buf = prnt_ans(cmd, 0);
          break;
        }

        // success!
        buf = prnt_ans(cmd, 1);
        break;

      case STOP:
        printf("STOP\n");
        cyc_count = MAX_CYCLE;
        break;

      default:
        printf("DO not know.\n");
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

  printf("\nStopping server\n");
  clean_up();
  return 0;
}

int clean_up()
{
  struct file_supervisor *fs = f_sv_getlist();

  printf("Cleaning all shared memory\n");
  // clean all files from shared memory
  if ( fs != NULL )
  {
    int i = 0;
    while (1)
    { //breaks if end of list reached

      if ( strncmp(fs->files[i], "/END",4) == 0)
        break;

      else if ( fs->files[i][0] !=  '\00' )
      {
        delete_shm_f(fs->files[i]);
        f_sv_del(fs->files[i]);
      } 

      i++;
    }
  }

  // clean file supervisor
  f_sv_clean_shm();
  return 0;
}
