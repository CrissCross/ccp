// Global variables
#define DEBUG_LEVEL 0 // 0 (disabled) - 5 (verbose)
#define F_LIMIT 256 // max number of files
#define F_MAX_LEN 100 // max length of a file name
#define MAX_CONTENT 1000 // max length of file content

enum cmds { LIST, CREATE, READ, UPDATE, DELETE, STOP };

struct cmd_info
{
  enum cmds cmd;
  char *fname;
  int content_len;
  char *content;
};

// f_supervisor.c -> Shared memory file supervisor 
int f_sv_setup_shm();
int f_sv_clean_shm();
int f_sv_add(char *fname);
int f_sv_del(char *fname);
int f_sv_addreader(char *fname);
int f_sv_delreader(char *fname);

struct file_supervisor
{
  // counts total amount of files
  int count;
  //
  // counts for each file how many are reading it.
  int reader_count[F_LIMIT];
  //
  // file names
  char files[F_LIMIT][F_MAX_LEN];

};

struct file_supervisor *f_sv_getlist();


// shm_f_actino.c -> Shared memory file actions
int create_shm_f(char *fname, char*fcontent);
int update_shm_f(char *fname, char *fcontent);
char *get_shm_f(char *fname);
int delete_shm_f(char *fname);

// fserver_io.c -> get input / write to output buffer
struct cmd_info *get_cmd();
char *prnt_ans (struct cmd_info *cinfo, int success);

// sem_f_action -> semaphore ops
int sem_create(char *fname);
int sem_dec_r(char *fname);
int sem_dec_w(char *fname);
int sem_inc_r (char* fname);
int sem_inc_w (char* fname);
int sem_kill (char* fname);
