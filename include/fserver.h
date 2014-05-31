// Global variables
#define F_LIMIT 256 // max number of files
#define F_MAX_LEN 100 // max length of a file name

struct file_supervisor
{
  char files[F_LIMIT][F_MAX_LEN];
};
