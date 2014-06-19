#define _GNU_SOURCE

#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() and inet_ntoa() */
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>      /* for printf() and fprintf() and ... */
#include <stdlib.h>     /* for atoi() and exit() and ... */
#include <string.h>     /* for memset() and ... */
#include <sys/socket.h> /* for socket(), bind(), recv, send(), and connect() */
#include <sys/types.h>
#include <unistd.h>     /* for close() */
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>


#include <helpers.h>
#include <fserver.h>

#define PORT 60606

#define MAX_CMDS_READ_PER_FILE 15      // max lines taken from an input file
#define MSG_CAP 4000                    // Message capacity / Max message size 

#define MAXPENDING 5    /* Maximum outstanding connection requests */

// global server info so server socket can be closed by signal handler
struct srv_info si;

int sem_trydec(sem_t *sem_addr);
int sem_inc(char *name);
int sem_dec(char *name);
int tcp_sem_create(char *name);
int tcp_sem_kill(char *name);
void* shm_create (char *name, size_t size);
int shm_kill (char *fname);
int add_child(char *name, int *pcc);
int rmv_child(char *name, int *pcc);


struct cmd_info *tcp_get_cmd(char *msg);

int handle_children(int cli_sock);
int handle_papa();
char* get_arg0(char *buf);
int get_content(struct cmd_info *ci, char *content);
int clean_up();

void usage(char *argv0, char *msg) {
  printf("%s\n", msg);
  printf("Usage:\n\n");
  printf("%s\n", argv0);
  printf(" starts a tcp file server that waits for incoming connections.\n");
  exit(1);
}

struct srv_info { // server socket info
        int srv_socket;                    /* Socket descriptor for server */
        struct sockaddr_in srv_addr; /* Local address */
};

// signal handler
static void handler(int signum)
{
        printf("[Ctrl]-[c] pressed! Waiting for children\n");
        int pid;
        int status;
        while (1)
        {
                pid = waitpid (WAIT_ANY, &status, WNOHANG);
                if (pid < 0) {
                        perror ("waitpid");
                        break;
                }
                if (pid == 0)
                        break;

                if(DEBUG_LEVEL > 0)
                        printf("Terminated child %d \n", pid);

        }
}

int main(int argc, char *argv[])
{
        int retcode;
        if (DEBUG_LEVEL > 0) printf("\n Welcome to the TCP/SHM File Servrer!\n\n");

        // set up shared mem for the file supervisor
        retcode = f_sv_setup_shm();
        handle_my_error(retcode, "Couldnt set up shm for file supervisor", PROCESS_EXIT);

        char *sem_name = "retnuoCsnoitcennoC";

        unsigned short server_port;     /* Server port */

        int cli_socket;                    /* Socket descriptor for client */
        struct sockaddr_in cli_addr; /* Client address */
        unsigned int cli_addr_len;            /* Length of client address data structure */

        struct sigaction sa;
        // redefine SIGINT handler
        sa.sa_handler = handler;
        sigemptyset(&sa.sa_mask);
        //// Restart functions if interrupted by handler 
        //// sa.sa_flags = SA_RESTART; 
        sa.sa_flags = 0; 
        retcode = sigaction(SIGTERM,&sa,NULL);
        handle_error(retcode, "Redefinition of SIGINT hanlder failed.", PROCESS_EXIT);


        // TODO -h is already mapped to heap sort
        // If (is_help_requested(argc, argv)) {
        //     usage(argv[0], "");
        // }

        if (DEBUG_LEVEL > 0) 
                printf("this much arguments: %d\n", argc);

        if (argc != 1) {
                usage(argv[0], "wrong number of arguments");
        }

        // TODO only one option should be allowed
        // Get sort algo option from argv
        while ((retcode = getopt(argc, argv, "h:")) != -1) {
                switch (retcode) {
                        case 'h' :
                                usage(argv[0], "wrong option");
                                break;
                        default:
                                usage(argv[0], "wrong option");
                                break;
                }
        }
        //
        // extract file content and give pointer to tpub.orig

        server_port = PORT;  /* First arg:  local port */

        /* Create socket for incoming connections */
        si.srv_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        handle_error(si.srv_socket, "socket() failed", PROCESS_EXIT);

        /* Construct local address structure */
        memset(&si.srv_addr, 0, sizeof(si.srv_addr));   /* Zero out structure */
        si.srv_addr.sin_family = AF_INET;                /* Internet address family */
        si.srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
        si.srv_addr.sin_port = htons(server_port);      /* Local port */

        int optval = 1;
        retcode = setsockopt(si.srv_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)); 
        handle_error(retcode, "setting recv timeout failed", PROCESS_EXIT);

        /* Bind to the local address */
        retcode = bind(si.srv_socket, (struct sockaddr *) &si.srv_addr, sizeof(si.srv_addr));
        handle_error(retcode, "bind() failed", PROCESS_EXIT);

        /* Mark the socket so it will listen for incoming connections */
        retcode = listen(si.srv_socket, MAXPENDING);
        handle_error(retcode, "listen() failed", PROCESS_EXIT);

        retcode = tcp_sem_create(sem_name);

        int *pcc =  (int *)shm_create(sem_name, sizeof(int)); // pending connections counter
        if (pcc == NULL)
                handle_error(-1, "Could not create shared memory", PROCESS_EXIT);

        // TODO add sigint signal handler to tidy up before finishing

        *pcc = 0;
        while (1) { // stop at signal or when STOP received
                if (DEBUG_LEVEL > 0) {
                        printf("\nCurrent pending connecstions: %d\n", (*(int *)pcc));
                        printf("Waiting for incoming connections.\n");
                }

                // check if max pending reached.
                if (*pcc >= MAXPENDING)
                {
                        printf("Max pending connections reached. Will retry after 1 sec.\n");
                        sleep(1);
                        continue;
                }

                cli_addr_len = sizeof(cli_addr);

                /* Wait for a client to connect */
                cli_socket = accept(si.srv_socket, (struct sockaddr *) &cli_addr, &cli_addr_len);
                if (cli_socket < 0 && errno == EINTR) { // accept was aborded by signal
                        if (DEBUG_LEVEL > 0) 
                                printf("Received signal...closing socket.\n");

                        retcode = close(si.srv_socket);
                        handle_error(retcode, "Closing Server socket", PROCESS_EXIT);

                        if (DEBUG_LEVEL > 0) 
                                printf("Server socket closed, cleaning up\n");

                        // deleting shared memory / semaphore
                        clean_up();
                        tcp_sem_kill(sem_name);
                        shm_kill(sem_name);
                        if (DEBUG_LEVEL > 0) 
                                printf("Bye!\n");
                        exit(0);
                }
                else if (cli_socket < 0)
                        handle_error(cli_socket, "accept() failed", PROCESS_EXIT);

                else if( cli_socket > 0)
                {
                        
                        if (DEBUG_LEVEL > 0)
                                printf("Opened socket #%d.\n", cli_socket);
                        /* client_socket is connected to a client! */
                        if (DEBUG_LEVEL > 0)
                                printf("Handling client %s\n", inet_ntoa(cli_addr.sin_addr));

                        retcode = fork();
                        handle_my_error(retcode, "fork failed", PROCESS_EXIT);
                        
                        if (retcode == 0) { // child
                                retcode = add_child(sem_name, pcc);
                                handle_my_error(retcode, "Could not add child", PROCESS_EXIT);
                                retcode = handle_children(cli_socket);
                                handle_my_error(retcode, "Could not handle child", NO_EXIT);
                                rmv_child(sem_name, pcc);

                                close(cli_socket);    /* Close client socket */
                                if (DEBUG_LEVEL > 0) 
                                        printf("Client closed\n");
                        }
                        else {
                                retcode = handle_papa();
                                exit(0);
                                break;
                        }
                }
        }

}

int handle_children(int cli_socket)
{
        int retc;
        int count;


        // send initial message
        char msg_buf[MSG_CAP];

        size_t len = strlen("READY");

        retc = write(cli_socket, "READY", len);
        if (retc !=  len)
                return handle_error(retc, "Could not write welcome message to socket", PROCESS_EXIT);

        int stop = 0;
        while(stop == 0) { // breaks if socket closed or STOP received 

                count = read(cli_socket, msg_buf, MSG_CAP - 1);

                if (count < 0) { // error
                        handle_error(count, "Could not read from socket", NO_EXIT);
                        break;
                }

                else if (count == 0) { // end of transmission
                        if (DEBUG_LEVEL > 0) printf("End of file\n");
                        break;
                }

                msg_buf[count] = '\00';

                char *buf = NULL;

                struct cmd_info *cmd = tcp_get_cmd(msg_buf);
                if (cmd == NULL) {
                        handle_my_error(-1, "Could not interpret command", NO_EXIT);
                        char * err = "ERRORINCMD\n";
                        write(cli_socket, err, strlen(err));
                        continue;
                }
                
                if (DEBUG_LEVEL > 0) 
                        printf("Read cpmmand: enum = %d fname = %s content len = %d....\n", (int)cmd->cmd, cmd->fname, cmd->content_len);
                //print_ans();


                switch ( cmd->cmd )
                { // LIST, CREATE, READ, UPDATE, DELETE, STOP
                        case LIST:
                                if (DEBUG_LEVEL > 0) printf("LIST\n");
                                buf = prnt_ans(cmd, 0);
                                break;

                        case CREATE:
                                if (DEBUG_LEVEL > 0) printf("CREATE\n");

                                retc = sem_create(cmd->fname);
                                if (retc < 0)
                                {
                                        buf = prnt_ans(cmd, 0);
                                        break;
                                }

                                retc = sem_dec_w(cmd->fname);
                                if (retc < 0)
                                {
                                        buf = prnt_ans(cmd, 0);
                                        break;
                                }

                                // add entry to file superisor
                                retc = f_sv_add(cmd->fname);
                                if ( retc < 0 )
                                {
                                        handle_my_error(retc, "Error CREATE: Adding file supervisor failed.", NO_EXIT);
                                        buf = prnt_ans(cmd, 0);
                                        sem_inc_w(cmd->fname);
                                        break;
                                }

                                // create shared memory
                                retc = create_shm_f(cmd->fname, cmd->content);
                                if ( retc < 0 )
                                {
                                        handle_my_error(retc, "Error CREATE: Couldnt create shared memory.", NO_EXIT);
                                        buf = prnt_ans(cmd, 0);
                                        sem_inc_w(cmd->fname);
                                        break;
                                }

                                // success!
                                //
                                sem_inc_w(cmd->fname);
                                buf = prnt_ans(cmd, 1);
                                break;

                        case READ:
                                if (DEBUG_LEVEL > 0) printf("READ\n");

                                if ( f_sv_find_file(cmd->fname) < 0 )
                                {
                                        buf = prnt_ans(cmd, 0);
                                        break;
                                }

                                // lock reader count semaphore (blocking) 
                                sem_dec_r(cmd->fname);
                                retc = f_sv_addreader(cmd->fname);
                                if ( retc == 1 )
                                { // we are the first reader, lets lock for writing
                                        retc = sem_dec_w(cmd->fname);
                                        if ( retc < 0 )
                                        { // cannot lock write semaphore. Obviously sb is busy on the file
                                                handle_my_error(-1, "READ: file is being changed, cannot read", NO_EXIT);
                                                buf = prnt_ans(cmd, 0);

                                                sem_inc_r(cmd->fname);
                                                f_sv_delreader(cmd->fname);
                                                sem_inc_r(cmd->fname);
                                                break;
                                        }
                                }
                                sem_inc_r(cmd->fname);

                                buf = prnt_ans(cmd, 1);

                                // finished reading
                                sem_dec_r(cmd->fname);
                                retc = f_sv_delreader(cmd->fname);
                                if ( retc == 0 ) // we were the last reader
                                        retc = sem_inc_w(cmd->fname);

                                sem_inc_r(cmd->fname);

                                break;

                        case UPDATE:
                                if (DEBUG_LEVEL > 0) printf("UPDATE\n");

                                if ( f_sv_find_file(cmd->fname) < 0 )
                                {
                                        buf = prnt_ans(cmd, 0);
                                        break;
                                }

                                // semaphore dekrementieren
                                retc = sem_dec_w(cmd->fname);
                                if (retc < 0)
                                {
                                        sem_inc_w(cmd->fname);
                                        buf = prnt_ans(cmd, 0);
                                        break;
                                }

                                retc = update_shm_f(cmd->fname, cmd->content);
                                if ( retc < 0 )
                                {
                                        handle_my_error(retc, "Error UPDATE: Couldnt update shared memory.", NO_EXIT);
                                        sem_inc_w(cmd->fname);
                                        buf = prnt_ans(cmd, 0);
                                        break;
                                }

                                // success!
                                sem_inc_w(cmd->fname);
                                buf = prnt_ans(cmd, 1);
                                break;

                        case DELETE:
                                if (DEBUG_LEVEL > 0) printf("DELETE\n");

                                // semaphore dekrementieren
                                retc = sem_dec_w(cmd->fname);
                                if (retc < 0)
                                {
                                        buf = prnt_ans(cmd, 0);
                                        break;
                                }

                                // delete entry on file supervisor
                                retc = f_sv_del(cmd->fname);
                                if ( retc < 0 )
                                {
                                        handle_my_error(retc, "Error DELETE: Removing file supervisor failed.", NO_EXIT);
                                        buf = prnt_ans(cmd, 0);
                                        sem_inc_w(cmd->fname);
                                        break;
                                }
                                // delete shared mem
                                retc = delete_shm_f(cmd->fname);
                                if ( retc < 0 )
                                {
                                        handle_my_error(retc, "Error DELETE: Couldnt delete shared memory.", NO_EXIT);
                                        buf = prnt_ans(cmd, 0);
                                        sem_inc_w(cmd->fname);
                                        break;
                                }
                                // delete semaphore
                                sem_kill(cmd->fname);

                                // success!
                                buf = prnt_ans(cmd, 1);
                                break;

                        case STOP:
                                if (DEBUG_LEVEL > 0) printf("STOP\n");
                                stop = 1;
                                char * err = "ERRORINCMD\n";
                                write(cli_socket, err, strlen(err));
                                break;

                        default:
                                if (DEBUG_LEVEL > 0) printf("DO not know.\n");
                                break;
                }


                if ( buf != NULL )
                { // we have to print an answer and to free it afterwards
                        // print answer to socket
                        count = write(cli_socket, buf, strlen(buf));
                        if (count <= 0)
                        {
                                handle_error(count, "Could not read from socket", NO_EXIT);
                                stop = 1;
                        }

                        //printf("%s\n", buf);
                        // answer buffer and cmd info struct not needed anymore
                        free(buf);
                }

                // clean cmd struct
                if (cmd->fname != 0)
                        free(cmd->fname);
                if (cmd->content != 0)
                        free(cmd->content);
                if (cmd != 0)
                        free(cmd);


        }
        if (DEBUG_LEVEL > 0) printf("Closing socket #%d .\n", cli_socket);
        return 0;
}

int add_child(char *name, int *pcc)
{
        int retc;
        retc = sem_dec(name);
        if (retc < 0)
        {
                handle_error(retc, "Coult not add child to the pending children counter", NO_EXIT);
                return -1;
        }

        (*pcc)++;
        sem_inc(name);
        return *pcc;
}

int rmv_child(char *name, int *pcc)
{
        int retc;
        retc = sem_dec(name);
        if (retc < 0)
        {
                handle_error(retc, "Coult not add child to the pending children counter", NO_EXIT);
                return -1;
        }

        (*pcc)--;
        sem_inc(name);
        return *pcc;
}

int tcp_sem_create(char *name)
{

        char sem_name[strlen(name) + 1];
        sprintf(sem_name, "/%s", name);

        // create new semaphore, return error if already there
        sem_t *semaddr = sem_open(sem_name, O_CREAT|O_EXCL, S_IRWXU, 1);
        if ( semaddr == SEM_FAILED )
        {
                handle_error(-1, "Could not create semaphore", NO_EXIT);
                return -1;
        }

        if (DEBUG_LEVEL > 1) printf("Created semaphore /%s\n",sem_name );

        return 0;
}

int tcp_sem_kill (char* name)
{
  int retcode;

  // semaphore name needs a extra slash in front and /_w termination 
  char sem_name[strlen(name) + 1];
  sprintf(sem_name, "/%s", name);

  if (DEBUG_LEVEL > 1) printf("Kill semaphore for file: /%s\n",name );

  // get semaphore
  sem_t *semaddr = sem_open(sem_name, 0);
  if (semaddr == SEM_FAILED)
  {
    handle_error(-1, "tcp_sem_kill: Cannot get semaphore", NO_EXIT);
    return -1;
  }

  retcode = sem_unlink(sem_name);
  if (retcode < 0)
  {
    handle_error(retcode, "tcp_sem_kill: Cannot unlink semahore", NO_EXIT);
    return -1;
  }

  retcode = sem_close(semaddr);
  if (retcode < 0)
  {
    handle_error(retcode, "tcp_sem_kill: Cannot close semahore", NO_EXIT);
    return -1;
  }
  return 0;
}

void* shm_create (char *name, size_t size)
{
        int fd;
        int retcode;

        char shm_name[strlen(name) + 1];
        sprintf(shm_name, "/%s", name);

        // create new shm segment, return error if already there
        fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
        if(handle_error(fd, "Could not create shm", NO_EXIT) == -1)
        {
                return NULL;
        }
        // resize freash shm to content size
        retcode = ftruncate(fd, sizeof(int));
        if(handle_error(retcode, "Could not truncate shm", NO_EXIT) == -1)
        {
                retcode = shm_unlink(shm_name);
                handle_error(retcode, "Could not unlink shm", NO_EXIT);
                return NULL;
        }

        // Map shared memory object
        void *shm_content = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if(shm_content == MAP_FAILED)
        {
                handle_error(-1, "mmap failed", NO_EXIT);
                retcode = shm_unlink(name);
                handle_error(retcode, "Could not unlink shm", NO_EXIT);
                return NULL;
        }
        else {
                handle_ptr_error(shm_content, "Alternative mmap error", NO_EXIT);
        }
        
        return shm_content;

}

int shm_kill (char *fname)
{ // read a file from shared memory

  int retcode;

  // shared memory name needs a extra slash in front
  char shm_fname[strlen(fname) + 1];
  sprintf(shm_fname, "/%s", fname);
  if (DEBUG_LEVEL > 1) printf("Delete file: %s\n",shm_fname );

  // get shm file descriptor by name
  int fd = shm_open(shm_fname, O_RDWR, 0);
  if(handle_error(fd, "Could not open shm", NO_EXIT) == -1)
  {
    return -1;
  }

  retcode = shm_unlink(shm_fname);
  if(handle_error(retcode, "Deletion failed", NO_EXIT) == -1)
  {
    return -1;
  }

  return 0;
}

int sem_inc (char *name)
{
  int retcode;
  // read semaphore name needs a extra slash in front and a  at the end
  char sem_name[strlen(name) + 1];
  sprintf(sem_name, "/%s", name);

  if (DEBUG_LEVEL > 1) printf("Increasing read semaphore: %s\n",name );

  // Get semaphore by name
  sem_t *sem_addr = sem_open(name, 0);

  retcode = sem_post(sem_addr);
  if (retcode < 0 )
  {
    handle_error(retcode, "Couldnt increment semaphore", NO_EXIT);
    return -1;
  }

  return 0;
}

int sem_dec(char *fname)
{
  int retcode;
  // read semaphore name needs a extra slash in front and a  at the end
  char sem_fname[strlen(fname) + 1];
  sprintf(sem_fname, "/%s", fname);

  // Get semaphore by name
  if (DEBUG_LEVEL > 1) printf("Decreasing semaphore: %s\n",sem_fname );
  sem_t *sem_addr = sem_open(sem_fname, 0);

  // try to decrement
  retcode = sem_wait(sem_addr);
  if (retcode < 0 )
  {
    handle_error(retcode, "Could not decrement semaphore", NO_EXIT);
    return -1;
  }

  return 0;
}

int handle_papa()
{
        if (DEBUG_LEVEL > 0) printf("Parent finished\n");
        return 0;
}
struct cmd_info *tcp_get_cmd(char *msg)
{
        char *cmd_line = NULL;
        struct cmd_info *cinfo = NULL;

        if (DEBUG_LEVEL > 1) printf("Reading lines:\n");

        // doubling msg thus we can do strsep ops
        char *tempbuf2 = strndup(msg, strlen(msg));
        if ( tempbuf2 == NULL ) {
                handle_ptr_error(tempbuf2, "tcp_get_cmd: strndub failed", NO_EXIT);
                return NULL;
        }

        // we need it to free tempbuf when done
        char *ptr2buf = tempbuf2;

        cmd_line = strsep(&tempbuf2, "\n");
        int cmd_line_len = strlen(cmd_line);

        // rest of msg is the content 
        char *content = tempbuf2;

        // Check if the command ends with \n
        if ( strncmp( &cmd_line[cmd_line_len-2], "\\n", 2) != 0)
        { 
                if (DEBUG_LEVEL > 0) printf("Command line must be terminated with '\\n' which is not the case.\n");
                return NULL;
        }

        // Cut "\\n" at the end
        cmd_line[cmd_line_len - 2] = '\00';

        char *cmd_ptr = cmd_line;
        //char *arg0 = cmd_line;

        // Separeate arg0 from arg1 and get ptr to arg1:
        //cmd_ptr = get_arg0(cmd_ptr);
        strsep(&cmd_ptr, " ");

        // valid arg0 
        if ( valid_cmd(cmd_line) == 0 )
        {
                if (DEBUG_LEVEL > 1) printf("get_arg0: command validation erroR\n");
                free(ptr2buf);
                return NULL;
        }

        char *cmd_snip = cmd_line;

        if (DEBUG_LEVEL > 0) printf("Befehl ist: %s und rest ist %s\n", cmd_snip, cmd_ptr);

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

                if ( cmd_ptr == NULL ){
                        handle_my_error(-1, "Arguments for CREATE missing", NO_EXIT);
                        free(cinfo);
                        cinfo = NULL;
                }

                // get file name
                else {
                        int ret = get_args(cmd_ptr, 2, cinfo);
                        if ( ret < 0 )
                        { // getargs failed
                                handle_my_error(-1, "Error  getting Aarguments", NO_EXIT);
                                free(cinfo);
                                cinfo = NULL;
                        }
                        // get content
                        ret = get_content(cinfo, content);
                        if ( ret < 0 )
                        { // getargs failed
                                handle_my_error(-1, "Couldnt get content", NO_EXIT);
                                free(cinfo);
                                cinfo = NULL;
                        }
                }
        }
        else if (strcmp(cmd_snip, "READ") == 0)
        { // READ, 1 argument
                cinfo->cmd = READ;

                if (cmd_ptr == NULL)
                {
                        if ( DEBUG_LEVEL > 0 ) printf("What file shall I read?\n");
                        free(cinfo);
                        cinfo = NULL;
                }
                else 
                {
                        int ret = get_args(cmd_ptr, 1, cinfo);
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
                int ret = get_args(cmd_ptr, 2, cinfo);
                if ( ret < 0 )
                { // getargs failed
                        handle_my_error(-1, "Couldnt get arguments", NO_EXIT);
                        free(cinfo);
                        cinfo = NULL;
                }
                // get content
                ret = get_content(cinfo, content);
                if ( ret < 0 )
                { // getargs failed
                        handle_my_error(-1, "Couldnt get content", NO_EXIT);
                        free(cinfo);
                        cinfo = NULL;
                }
        }
        else if (strcmp(cmd_snip, "DELETE") == 0)
        { // DELETE, 1 argumnet
                cinfo->cmd = DELETE;

                // get file name
                int ret = get_args(cmd_ptr, 1, cinfo);
                if ( ret < 0 )
                { // getargs failed
                        handle_my_error(-1, "Couldnt get arguments", NO_EXIT);
                        free(cinfo);
                        cinfo = NULL;
                }
        }
        else if (strcmp(cmd_snip, "STOP") == 0)
        { // STOP, 1 argumnet
                printf("REceived STOP\n");
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

        if ( ptr2buf != NULL ) free(ptr2buf);

        if (DEBUG_LEVEL > 1) printf("\n");


        return cinfo;

};

int get_content(struct cmd_info *ci, char *content)
{
        if(content == NULL)
        {
                printf("get_content: Content missing\n");
                return -1;
        }

        int realen = strlen(content);
        if (realen < ci->content_len)
        {
                printf("get_content: Content is shorter than the given length\n");
                return -1;
        }
        ci->content = calloc(ci->content_len + 1, sizeof(char));
        strncpy(ci->content, content, ci->content_len);
        ci->content[ci->content_len] = '\00';

        return 0;
}

int clean_up()
{
  struct file_supervisor *fsv = f_sv_getlist();

  if (DEBUG_LEVEL > 0) printf("Cleaning all shared memory\n");
  // clean all files from shared memory
  if ( fsv != NULL )
  {
    int i = 0;
    while (1)
    { //breaks if end of list reached

      if ( strncmp(fsv->files[i], "/END",4) == 0)
        break;

      else if ( fsv->files[i][0] !=  '\00' )
      {
        delete_shm_f(fsv->files[i]);
        sem_kill(fsv->files[i]);
        f_sv_del(fsv->files[i]);

      } 

      i++;
    }
  }

  // clean shared memory for file supervisor
  f_sv_clean_shm();
  return 0;
}
