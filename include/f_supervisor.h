// Shared memory file supervisor 
int f_sv_setup_shm();
int f_sv_clean_shm();
int f_sv_add(char *fname);
int f_sv_del(char *fname);
struct file_supervisor *f_sv_getlist();

