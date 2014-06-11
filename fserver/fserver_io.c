#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // isalnum
#include <limits.h> // strtonum
#include <bsd/stdlib.h> // strtonum

#include <helpers.h>
#include <fserver.h>

// max length of one command line
#define CMD_LINE_LEN 200
// max lines taken from an input file
#define MAX_LINES_READ_PER_FILE 15
// Length of buffer that hold the answer
#define MAX_ANSW_LEN 3000

int valid_cmd (char *str);

int valid_fname(char *str);

int get_args (char *cmd_snip, int args_needed, struct cmd_info *cargs);

char *prnt_list();

struct cmd_info *get_cmd()
{
  char *cmd_line = NULL;
  size_t len = 0;
  ssize_t read;
  struct cmd_info *cinfo = NULL;

  int i = 0;
  if (DEBUG_LEVEL > 1) printf("Reading lines:\n");

  while( i < MAX_LINES_READ_PER_FILE )
  { // read max 15 lines
    read = getline(&cmd_line, &len, stdin);
    if(handle_my_error(read, "Error reading input", NO_EXIT) == -1)
    {
      //cinfo == NULL;
      break;
      //return NULL;
    }

    if (read == 0)
    {
      if (DEBUG_LEVEL > 1) printf("File is empty\n\n");
      break;
      //return NULL;
    }
    else if (strcmp(cmd_line,"\n") == 0)
    {
      if (DEBUG_LEVEL > 1) printf("%d\tempty line\n", i);
      i++;
      continue;
    }
    if (read > 0 && read < CMD_LINE_LEN)
    {
      if (DEBUG_LEVEL > 1) printf("%d\tread %d chars: %s", i, (int) read, cmd_line);

      // Check if the command ends with \n
      if ( strncmp( &cmd_line[read-3], "\\n", 2) != 0)
      { 
        if (DEBUG_LEVEL > 1) printf("Command line must be terminated with '\\n' which is not the case.\n");
        break;
      }


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

      if (DEBUG_LEVEL > 1) printf("Befehl ist: %s und rest ist %s\n", cmd_snip, tempbuf);

      // Check if command is Uppercase if not, break
      if ( valid_cmd(cmd_snip) == 0 )
      {
        if (DEBUG_LEVEL > 1) printf("Unknown command\n");
        free(free_tempbuf);
        break;
      }

      // cmd seems to be valid. allocate mem for cmd struct
      cinfo = (struct cmd_info *) malloc(sizeof(struct cmd_info));
      
      // Content comes later
      cinfo->content = NULL;


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
          free(cinfo);
          cinfo = NULL;
        }
      }
      else if (strcmp(cmd_snip, "READ") == 0)
      { // READ, 1 argument
        cinfo->cmd = READ;

        if (tempbuf == NULL)
        {
          if ( DEBUG_LEVEL > 0 ) printf("What file shall I read?\n");
          free(cinfo);
          cinfo = NULL;
        }
        else 
        {
          int ret = get_args(tempbuf, 1, cinfo);
          if ( ret < 0 )
          { // getargs failed
            handle_my_error(-1, "Couldnt get arguments", NO_EXIT);
            free(cinfo);
            cinfo = NULL;
          }
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
          free(cinfo);
          cinfo = NULL;
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
          free(cinfo);
          cinfo = NULL;
        }
      }
      else if (strcmp(cmd_snip, "STOP") == 0)
      { // DELETE, 1 argumnet
        cinfo->cmd = STOP;
        cinfo->fname = NULL;
        cinfo->content_len = 0;
      }
      else
      { // non of the known commands
        handle_my_error(-1, "Unknown command", NO_EXIT);
        free(cinfo);
        cinfo = NULL;
      }

      if ( free_tempbuf != NULL ) free(free_tempbuf);
      break;
    }

  }
  
  if (DEBUG_LEVEL > 1) printf("\n");
  free(cmd_line);

  return cinfo;

};

int get_args (char *cmd_snip, int args_needed, struct cmd_info *cinfo)
{
  if (DEBUG_LEVEL > 1) printf("Show snippet: %s\n", cmd_snip);
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

char *prnt_ans (struct cmd_info *cinfo, int success)
{ // prints answer to buffer
  char *buf = NULL;
  switch ( cinfo->cmd )
  { // LIST, CREATE, READ, UPDATE, DELETE
      case LIST:
        buf = prnt_list();
        if (buf == NULL) handle_my_error(-1, "Error Writng file to buffer", NO_EXIT);
        break;
      case CREATE:
        if(success)
        { // 
          char *tempbuf = "FILECREATED\\n";
          buf = calloc(strlen(tempbuf), sizeof(char));
          sprintf(buf, tempbuf);
        }
        else
        {
          char *tempbuf = "FILEEXISTS\\n";
          buf = calloc(strlen(tempbuf), sizeof(char));
          sprintf(buf, tempbuf);
        }
        break;
      case READ:
        if(success)
        { // 
          buf = calloc(MAX_ANSW_LEN, sizeof(char));

          char *content = get_shm_f(cinfo->fname);
          if ( content == NULL) break;

          int content_len = strlen(content);
          snprintf(buf, MAX_ANSW_LEN, "FILECONTENT %s %d\n%s\n", cinfo->fname, content_len, content);
          buf = realloc(buf, strlen(buf)); 
        }
        else
        {
          char *tempbuf = "NOSUCHFILE\\n";
          buf = calloc(strlen(tempbuf), sizeof(char));
          sprintf(buf, tempbuf);
        }
        break;
      case UPDATE:
        if(success)
        { // 
          char *tempbuf = "UPDATED\\n";
          buf = calloc(strlen(tempbuf), sizeof(char));
          sprintf(buf, tempbuf);
        }
        else
        {
          char *tempbuf = "NOSUCHFILE\\n";
          buf = calloc(strlen(tempbuf), sizeof(char));
          sprintf(buf, tempbuf);
        }
        break;
      case DELETE:
        if(success)
        { // 
          char *tempbuf = "DELETED\\n";
          buf = calloc(strlen(tempbuf), sizeof(char));
          sprintf(buf, tempbuf);
        }
        else
        {
          char *tempbuf = "NOSUCHFILE\\n";
          buf = calloc(strlen(tempbuf), sizeof(char));
          sprintf(buf, tempbuf);
        }
        break;
      case STOP:
        break;
      default:
        break;
  }

  return buf;
}

int valid_cmd (char *str)
{ // validate a command (Uppercase letter, not longer than "UPDATE")
  int str_len = strlen(str);
  if (str_len > strlen("UPDATE"))
  {
    handle_my_error(-1, "Command is too long", NO_EXIT);
    return 0;
  }

  // in case last char is a 0-terminator, do not check last byte
  if ( str[str_len-1] == '\0' || str[str_len-1] == '\00' )
    str_len--;

  for ( int i = 0; i < str_len; i++)
  {
    if ( isalpha(str[i]) == 0 || isupper(str[i] == 0) )
    {
      handle_my_error(-1, "Command is not uppercasealphabetic", NO_EXIT);
      return 0;
    }
  }

  return 1;
}

int valid_fname(char *str)
{ // validate a file name
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

char *prnt_list()
{ // print list to buffer and return it
  char *buf = NULL;
  struct file_supervisor *superv = f_sv_getlist();
  if ( superv == NULL )
  {
    char *tempbuf = "NOFILES\\n";
    buf = calloc(strlen(tempbuf), sizeof(char));
    sprintf(buf, tempbuf);
    return buf;
  }

  if (superv->count == 0)
  {
    char *tempbuf = "There are no files at the moment.\\n";
    buf = calloc(strlen(tempbuf), sizeof(char));
    sprintf(buf, tempbuf);
    return buf;

  }
  
  int cur_pos;
  int bytes_written;
  int bytes_left;

  buf = calloc(MAX_ANSW_LEN, sizeof(char));
  cur_pos = snprintf(buf, MAX_ANSW_LEN, "ACK %d\n", superv->count);
  // sNprintf writes a '\0' char behind last char!!
  //cur_pos--;
  bytes_left = MAX_ANSW_LEN - cur_pos;
  int i = 0;
  int files_found = 0;
  while (1)
  { //breaks if end of list reached 

    if (strncmp(superv->files[i], "/END", 4) == 0 || files_found >= superv->count)
    {
      if ( DEBUG_LEVEL > 1 ) printf("this file: %s, next file: %s\n", superv->files[i], superv->files[i+1]); 
      break;
    }

    if ( superv->files[i][0] != '\00' )
    {
      files_found++;
      bytes_written = snprintf(&buf[cur_pos], bytes_left, "%d \t %s\n", i, superv->files[i]);
      if (bytes_written >= bytes_left)
      { // buffer is full, input string was truncated!
        handle_my_error(-1, "print file list to buffer: buffer full", NO_EXIT);
        break;
      }
      cur_pos = cur_pos + bytes_written;
      bytes_left = MAX_ANSW_LEN - cur_pos;
    }
    i++;

  }
  buf = realloc(buf, cur_pos +1);
  return buf;
}
