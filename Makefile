## Concurrent C Progrmming File Server
## Cristoffel Gehring
##

CC = gcc
CFLAGS = -Wall -g -O2 -std=gnu99 -I include -lrt -lbsd

objects = server_ctrl.o fserver_io.o f_supervisor.o shm_f_action.o error_handler.o

fserver_app : $(objects)
	cc $(CFLAGS) -o fserver_app $(objects)

server_ctrl.o : fserver/server_ctrl.c
	cc $(CFLAGS) -c fserver/server_ctrl.c 

fserver_io.o : fserver/fserver_io.c
	cc $(CFLAGS) -c fserver/fserver_io.c

f_supervisor.o : fserver/f_supervisor.c
	cc $(CFLAGS) -c fserver/f_supervisor.c 

shm_f_action.o : fserver/shm_f_action.c
	cc $(CFLAGS) -c fserver/shm_f_action.c

error_handler.o : lib/error_handler.c 
	cc $(CFLAGS) -c lib/error_handler.c

clean :
	rm fserver $(objects)

#clean:
#	rm -f cscope.out makeOut/*
#
## Cscope
#csclean:
#	rm -vf cscope.out
#
#cscope.out:
#	cscope -Rbqv
