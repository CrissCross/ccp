#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // isalnum
#include <limits.h> // strtonum
#include <bsd/stdlib.h> // strtonum

#include <helpers.h>
#include <fserver.h>

#define CMD_LEN 200




int valid_fname(char *str);

int get_args (char *cmd_snip, int args_needed, struct cmd_info *cargs);

struct cmd_info *get_cmd()
{
  char *cmd_line = NULL;
  size_t len = 0;
  ssize_t read;
  struct cmd_info *cinfo = NULL;

  int i = 0;
  printf("Reading lines:\n");
  while(i<15)
  { // read max 15 commands
    read = getline(&cmd_line, &len, stdin);
    if(handle_my_error(read, "Error reading input", NO_EXIT) == -1)
      return NULL;

    if (read == 0)
    {
      printf("File is empty\n\n");
      return NULL;
    }
    else if (strncmp(cmd_line,"STOP",4) == 0)
    {
      printf("%d\tSTOPP\n", i);
      i++;
      break;
    }
    else if (strcmp(cmd_line,"\n") == 0)
    {
      printf("%d\tempty line\n", i);
      i++;
      continue;
    }
    if (read > 0 && read < CMD_LEN)
    {
      // allocate mem for cmd struct
      cinfo = (struct cmd_info *) malloc(sizeof(struct cmd_info));
      // Content comes later
      cinfo->content = NULL;
      printf("%d\tread %d chars: %s", i, (int) read, cmd_line);

      // length of cmd line without "\n" at the end
      int rel_len = (int) read - 2;

      // copy line and cut '/n' at the end:
      char line[rel_len];
      strncpy((char *)line, cmd_line, rel_len);
      line[rel_len - 1] = '\00';

      // create tempbuf for parameter seperation process
      char *tempbuf = strdup(line);
      // pointer to start of char array to free allocated memory when finished
      char *free_tempbuf = tempbuf;

      // Let's see if there are parameters:
      char *cmd_snip = strsep(&tempbuf, " ");
      printf("Befehl ist: %s und rest ist %s\n", cmd_snip, tempbuf);

      if (strcmp(cmd_snip, "LIST") == 0)
      {
        cinfo->cmd = LIST;
        cinfo->fname = NULL;
        cinfo->content_len = 0;
      }
      else if (strncmp(cmd_snip, "CREATE",6) == 0)
      { // CREATE, 2 arguments
        cinfo->cmd = CREATE;

        // get file name
        int ret = get_args(tempbuf, 2, cinfo);
        if ( ret < 0 )
        { // getargs failed
          handle_my_error(-1, "Couldnt get arguments", NO_EXIT);
          return NULL;
        }
        

      }
      else if (strcmp(cmd_snip, "READ") == 0)
      { // READ, 1 argument
        cinfo->cmd = READ;

        int ret = get_args(tempbuf, 1, cinfo);
        if ( ret < 0 )
        { // getargs failed
          handle_my_error(-1, "Couldnt get arguments", NO_EXIT);
          return NULL;
        }
        

      }
      else if (strcmp(cmd_snip, "UPDATE") == 0)
      { // UPDATE, 2 arguments
        cinfo->cmd = UPDATE;
       
        // get file name
        int ret = get_args(tempbuf, 2, cinfo);
        if ( ret < 0 )
        { // getargs failed
          handle_my_error(-1, "Couldnt get arguments", NO_EXIT);
          return NULL;
        }
        

      }
      else if (strcmp(cmd_snip, "DELETE") == 0)
      { // DELETE, 1 argumnet
        cinfo->cmd = DELETE;
       
        // get file name
        int ret = get_args(tempbuf, 1, cinfo);
        if ( ret < 0 )
        { // getargs failed
          handle_my_error(-1, "Couldnt get arguments", NO_EXIT);
          return NULL;
        }
        

      }
      else
      {
        handle_my_error(-1, "Unknown command", NO_EXIT);
        free(cinfo);
        free(free_tempbuf);
        return NULL;
      }

      // cpy and replace last char with '\00'
      free(free_tempbuf);
      break;
    }

  }
  
  printf("\n");
  free(cmd_line);

  return cinfo;

};

int print_f_asread ()
{
  char *cmd_line = NULL;
  size_t len = 0;
  ssize_t read;

  int i = 0;
  while(i<15)
  {
    
    read = getline(&cmd_line, &len, stdin);

    if (read > 0 && read < CMD_LEN)
    {
      if(strcmp(cmd_line,"\n") == 0)
        printf("%d\t\n",i);
      else
        printf("%d\t%s",i, cmd_line);
    }
    else 
      printf("%i\tNothing was read\n",i);

    i++;
  }
  
  free(cmd_line);
  return 0;

};

int get_args (char *cmd_snip, int args_needed, struct cmd_info *cinfo)
{
  printf("Show snippet: %s\n", cmd_snip);
  // create tempbuf for parameter seperation process
  char *tempbuf = strdup(cmd_snip);
  // pointer to start of char array to free allocated memory when finished
  char *free_tempbuf = tempbuf;

  // Let's see if there are parameters:
  cmd_snip = strsep(&tempbuf, " ");
  if (tempbuf == NULL && args_needed == 2)
  {
    handle_my_error(-1, "No arguments found in cmd", NO_EXIT);
    free(free_tempbuf);
    return -1;
  }
  if(valid_fname(cmd_snip) == 0)
  {
    handle_my_error(-1, "Invalid name", NO_EXIT);
    free(free_tempbuf);
    return -1;
  }

  // fname seems to be valid, lets copy it to the struct
  int len = strlen(cmd_snip);
  int new_len = len;
  // check if string is terminated by a \0. if not add it 
  if ( cmd_snip[len-1] != '\0' || cmd_snip[len-1] != '\00' )
  {
    new_len++;
  }
  cinfo->fname = calloc(new_len,sizeof(char));
  strncpy((char *)cinfo->fname, cmd_snip, len);
  // make sure that the string has a correct ending
  cinfo->fname[new_len-1] = '\00';

  if( args_needed == 1 )
  { // we are finished
    free(free_tempbuf);
    return 0;
  }

  cmd_snip = strsep(&tempbuf, " ");
  if (tempbuf == NULL)
  { //TODO
  }

  const char *err_msg;
  int content_len = (int) strtonum(cmd_snip, 0, MAX_CONTENT, &err_msg);
  if (err_msg)
  { // something went wrong
    handle_error(-1, err_msg, NO_EXIT);
    free(free_tempbuf);
    return -1;
  }

  // fname seems to be valid, lets copy it to the struct
  cinfo->content_len = content_len;

  return 0;
}



int valid_fname(char *str)
{
  int str_len = strlen(str);
  if (str_len > F_MAX_LEN)
  {
    handle_my_error(-1, "file name is too long", NO_EXIT);
    return 0;
  }

  // in case last char is a 0-terminator, do not check last byte
  if ( str[str_len-1] == '\0' || str[str_len-1] == '\00' )
    str_len--;

  for ( int i = 0; i < str_len; i++)
  {
    if ( isalnum(str[i]) == 0)
    {
      handle_my_error(-1, "file name is not alphanumeric", NO_EXIT);
      return 0;
    }
  }

  return 1;
}

