## Hausaufgaben Systemprogrammierung ##
## Woche 3: Signals ##
#
ALLFLAGS=-Wall -g -O2 -std=gnu99 -I./include -L./lib

# LIBS=-lpthread -lerror_handling 
LIBS= -lhelpers -lfserver

clean:
	rm -f cscope.out makeOut/*

fserver/shm_admin: fserver/shm_admin.c lib/libhelpers.a include/helpers.h
	gcc $(ALLFLAGS) fserver/shm_admin.c -o makeOut/shm_admin -lrt -lhelpers

# Cscope
csclean:
	rm -vf cscope.out

cscope.out:
	cscope -Rbqv

ue/semashm : ue/semashm.c lib/libhelpers.a include/helpers.h lib/libfserver.a include/fserver.h
	gcc $(ALLFLAGS) ue/semashm.c $(LIBS) -o makeOut/semashm

ue/semashm_muloe : ue/semashm_muloe.c
	gcc $(ALLFLAGS) -lm $(LIBS) -o makeOut/semashm_muloe.c ue/semashm_muloe.c

# libs
#
##
lib/error_handler.o: lib/error_handler.c
	gcc -c $(ALLFLAGS) lib/error_handler.c -o lib/error_handler.o
#
##
fserver/shm_admin.o: fserver/shm_admin.c
	gcc -c $(ALLFLAGS) fserver/shm_admin.c -o fserver/shm_admin.o
#
## Build lib
##
lib/libhelpers.a: lib/error_handler.o
	ar crs lib/libhelpers.a lib/error_handler.o
##
lib/libfserver.a: fserver/shm_admin.o
	ar crs lib/libfserver.a fserver/shm_admin.o
