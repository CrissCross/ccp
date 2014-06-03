#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <shm_f_action.h>
#include <fserver.h>
#include <f_supervisor.h>
#include <fserver_io.h>
#include <helpers.h>

int clean_mem(struct cmd_info *cinfo);
int print_curr_files()
{
  struct file_supervisor *superv = f_sv_getlist();
  if (superv->count == 0)
  {
    printf("There are no files at the moment.\n");
    return 0;
  }
  
  printf("We have %d files:\n", superv->count);
  for (int i=0; i < superv->count; i++) 
  { 

    if (strncmp(superv->files[i], "/END", 4) == 0)
      break;

    if ( superv->files[i][0] != '\00' )
    {
      printf("%d \t %s\n", superv->count, superv->files[i]);
    }

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

  int max_cycle = 10;
  int cyc_count = 0;
  while (1)
  { // break if max_cycle reached
    printf("Cycle count: round %d\n", cyc_count-1);
    if(cyc_count > max_cycle) break;
    cyc_count++;

    struct cmd_info *cmd = get_cmd();
    if ( cmd == NULL )
    {
      printf("Error while getting command.\n\n");
      continue;
    }

    printf("Read cpmmand: enum = %d fname = %s content len = %d....\n", (int)cmd->cmd, cmd->fname, cmd->content_len);
    char *shm_content;
    switch ( cmd->cmd )
    { // LIST, CREATE, READ, UPDATE, DELETE
      case LIST:
        printf("LIST\n");
        print_curr_files();
        break;

      case CREATE:
        printf("CREATE\n");
        retcode = f_sv_add(cmd->fname);
        create_shm_f(cmd->fname, "place holder");
        break;

      case READ:
        printf("READ\n");
        shm_content = get_shm_f(cmd->fname);
        printf("read  content:\n%s\n\n", shm_content);
        break;

      case UPDATE:
        printf("UPDATE\n");
        update_shm_f(cmd->fname, "Updated place holder");
        shm_content = get_shm_f(cmd->fname);
        printf("read new content:\n%s\n\n", shm_content);
        break;

      case DELETE:
        printf("DELETE\n");
        delete_shm_f(cmd->fname);
        break;

      default:
        printf("DO not know.\n");
        break;
    }
    clean_mem(cmd);
  }

  f_sv_clean_shm();
  return 0;
}

int clean_mem(struct cmd_info *cinfo)
{
  free(cinfo->fname);
  //free(cinfo->content);
  free(cinfo);
  return 0;
}
