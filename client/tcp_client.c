#define _GNU_SOURCE
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>      /* for printf() and fprintf() and ... */
#include <stdlib.h>     /* for atoi() and exit() and ... */
#include <string.h>     /* for memset() and ... */
#include <unistd.h>     /* for close() */
#include <ctype.h> // isalnum

#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <sys/types.h>

#include <helpers.h>

#define PORT 60606
#define DEBUG_LEVEL 0

#define MAX_CMDS_READ_PER_FILE 15      // max lines taken from an input file
#define MSG_CAP 4000                    // Message capacity / Max message size 

int valid_ans (char *str);
int main(int argc, char *argv[]) {
        int retcode;

        int sock;                             // Socket descriptor
        struct sockaddr_in server_address;    // server address
        unsigned short server_port = (unsigned short) PORT;  // server port
        char *server_ip = "127.0.0.1";        // Server IP address (dotted quad)
        char msg_buf[MSG_CAP];                  // buffer to send/recv  

        char *cmd_line = NULL;
        ssize_t count = 0;
        int count_tot = 0;

        int i = 0;

        // Create a reliable, stream socket using TCP 
        sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        handle_error(sock, "socket() failed", PROCESS_EXIT);

        // Construct the server address structure
        memset(&server_address, 0, sizeof(server_address));   // Zero out structure
        server_address.sin_family      = AF_INET;             // Internet address family
        server_address.sin_addr.s_addr = inet_addr(server_ip);// Server IP address
        server_address.sin_port        = htons(server_port);  // Server port: htons host to network byte order

        // Establish the connection to the square server
        retcode = connect(sock, (struct sockaddr *) &server_address, sizeof(server_address));
        handle_error(retcode, "connect() failed", PROCESS_EXIT);
        if (DEBUG_LEVEL > 0)
                printf("Checking if server is ready...\n");
        
        count = read(sock, msg_buf, MSG_CAP - 1); 
        handle_error(count, "Server read failed\n", PROCESS_EXIT);
        msg_buf[count] = '\00';
        if (DEBUG_LEVEL > 0 ) 
                printf("Read:\n%s\n", msg_buf);


        int msg_valid = valid_ans(msg_buf);
        int srv_rdy = (strncmp(msg_buf, "READY", strlen("READY")) == 0);

        if ( !(msg_valid && srv_rdy) )
                handle_my_error(-1, "Server is not ready", PROCESS_EXIT);

        if (DEBUG_LEVEL > 0) printf("Server is readey!\n");

        size_t len;
        while( i < MAX_CMDS_READ_PER_FILE ) { // breaks if  max lines per file are read

                len = 0;
                if ( cmd_line != NULL)
                        free(cmd_line);

                // get line -> cmd_line will also contain newline char if it exists
                count = (int) getline(&cmd_line, &len, stdin);
                if (DEBUG_LEVEL > 1) printf("Command nr: %d, count: %d\n", i, (int) count);

                // check if the cmd is ready to send
                int cmd_ready = (strcmp(cmd_line,"\n") == 0) || (count <= 0);

                if (cmd_ready && (count_tot == 0) ) { // exit
                        if (DEBUG_LEVEL > 0) printf("End of file\n");
                        break;

                }
                else if (cmd_ready && (count_tot > 0)) { // send message

                        printf("\nSending to server:\n%s\n", msg_buf);
                        count = send(sock, msg_buf, count_tot, 0);
                        if (count != count_tot)
                                handle_my_error(-1, "send() sent a different number of bytes than expected",
                                                PROCESS_EXIT);

                        else if (DEBUG_LEVEL > 0)
                                printf("Sent this message:\n>%s<\n", msg_buf);

                        count_tot = 0;
                        msg_buf[0] = '\00';
                        i++;

                }
                else if ( count_tot + count > MSG_CAP) { // discard message

                        if (DEBUG_LEVEL > 0) 
                                printf("Your command is too long. Please shorten to %d characters\n", MSG_CAP);

                        count_tot = 0;
                        msg_buf[0] = '\00';
                        i++;
                        continue;
                        
                }
                else { // adding line to message
                        strncpy(&msg_buf[count_tot], cmd_line, count);
                        count_tot = count + count_tot;
                        msg_buf[count_tot] = '\00';
                        if (DEBUG_LEVEL > 1)
                                printf("buffer: %s\n", msg_buf);
                        continue;
                }

                count = 0;

                // get answer
                if (DEBUG_LEVEL > 0)
                        printf("Receiving answer from server..\n");
                count = recv(sock, msg_buf, MSG_CAP - 1, 0);
                handle_error( count, "Reception of answer failed.", PROCESS_EXIT);

                msg_buf[count] = '\00';
                printf("...Server answered:\n%s\n", msg_buf);

        }

        if ( cmd_line != NULL)
                free(cmd_line);
        printf("Finished, closing socket\n");
        shutdown(sock, SHUT_RDWR);
        printf("Socket closed, bye bye!\n");
        exit(0);
}
int valid_ans (char *str)
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
