#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <shm_f_action.h>
#include <fserver.h>
#include <f_supervisor.h>
#include <helpers.h>

void print_curr_files()
{
  int success = 0;
  struct file_supervisor *superv = f_sv_getlist();
  printf("\nFile list:\n");
  for (int i=0; i < F_LIMIT; i++) 
  { 

    if (strncmp(superv->files[i], "/END", 4) == 0)
      break;

    if ( superv->files[i][0] != '\0' )
    {
      printf("%d \t %s\n", success+1, superv->files[i]);
      success++;
    }

  }

  if (success == 0)
    printf("There are no files at the moment.\n");


  printf("\n");
}
  
int main (int argc, char **argv)
{
  int retcode;
  char *fname = "cheerup";
  char *fcontent = "FCB FCB FCB!";
  char *fnewcontent = "Nananana Nananana Ehhh Ehhh Ehhh FCB!";
  char *f2name = "breaktime";
  char *f2content = "Drink beer";

  retcode = f_sv_setup_shm();
  handle_my_error(retcode, "Couldnt set up shm for file supervisor", NO_EXIT);

  retcode = f_sv_add(fname);
  create_shm_f(fname, fcontent);
  char *shm_content = get_shm_f(fname);
  printf("read  content:\n%s\n\n", shm_content);

  update_shm_f(fname, fnewcontent);
  shm_content = get_shm_f(fname);

  printf("read new content:\n%s\n\n", shm_content);

  retcode = f_sv_add(f2name);
  handle_my_error(retcode, "adding 2nd file to supervisor", NO_EXIT);
  create_shm_f(f2name, f2content);

  shm_content = get_shm_f(f2name);
  printf("read content of %s:\n%s\n\n", f2name, shm_content);

  print_curr_files();

  delete_shm_f(fname);
  f_sv_del(fname);

  print_curr_files();

  delete_shm_f(f2name);
  f_sv_del(f2name);

  print_curr_files();
  
  f_sv_clean_shm();

  return 0;
}
