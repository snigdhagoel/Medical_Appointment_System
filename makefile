CC = gcc
CFLAGS = -lsocket -lresolv -lnsl

all: healthcenterserver doctor1 doctor2 patient1 patient2

healtcenterserver: healthcenterserver.c 
		$(CC) healthcenterserver.c $(CFLAGS) -o healthcenterserver

doctor1: doctor1.c
	$(CC) doctor1.c $(CFLAGS) -o doctor1

doctor2: doctor2.c
	$(CC) doctor2.c $(CFLAGS) -o doctor2

patient1: patient1.c
	$(CC) patient1.c $(CFLAGS) -o patient1

patient2: patient2.c
	$(CC) patient2.c $(CFLAGS) -o patient2

clean:
	rm -rf healthcenterserver doctor1 doctor2 patient1 patient2
