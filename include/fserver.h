// Global variables
#define F_LIMIT 256 // max number of files
#define F_MAX_LEN 100 // max length of a file name
#define MAX_CONTENT 1000 // max length of file content

enum cmds { LIST, CREATE, READ, UPDATE, DELETE };

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

