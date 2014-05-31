enum exit_type { PROCESS_EXIT, THREAD_EXIT, NO_EXIT };
/* helper function for dealing with errors */
int handle_error(long return_code, const char *msg, enum exit_type et);

int handle_my_error(long return_code, const char *msg, enum exit_type et);

void handle_error_myerrno(long return_code, int myerrno, const char *msg, enum exit_type et);

void handle_ptr_error(void *ptr, const char *msg, enum exit_type et);

void die_with_error(char *error_message);

