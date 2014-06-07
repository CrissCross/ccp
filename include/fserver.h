// Global variables
#define DEBUG_LEVEL 0 // 0 (disabled) - 5 (verbose)
#define F_LIMIT 256 // max number of files
#define F_MAX_LEN 100 // max length of a file name
#define MAX_CONTENT 1000 // max length of file content

enum cmds { LIST, CREATE, READ, UPDATE, DELETE, STOP };

struct file_supervisor
{
  int count;
  char files[F_LIMIT][F_MAX_LEN];
};

struct cmd_info
{
  enum cmds cmd;
  char *fname;
  int content_len;
  char *content;
};

// shm_f_actino.c -> Shared memory file actions
int create_shm_f(char *fname, char*fcontent);
int update_shm_f(char *fname, char *fcontent);
char *get_shm_f(char *fname);
int delete_shm_f(char *fname);

// f_supervisor.c -> Shared memory file supervisor 
int f_sv_setup_shm();
int f_sv_clean_shm();
int f_sv_add(char *fname);
int f_sv_del(char *fname);
struct file_supervisor *f_sv_getlist();

// fserver_io.c -> get input / write to output buffer
struct cmd_info *get_cmd();
char *prnt_ans (struct cmd_info *cinfo, int success);
