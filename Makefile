CC=g++
CFLAGS=-Wall -g -lpthread
#OBJ = frontendserver.o servercommon.o singleconnserverhtml.o \
#singleconnservercontrol.o cookierelay.o backendrelay.o mailservice.o #loadbalancer.o

TARGETS = servercommon frontendserver singleconnserverhtml singleconnservercontrol \
	cookierelay backendrelay mailservice

all: $(TARGETS)

servercommon: servercommon.o
	$(CC) $^ $(CFLAGS) -o $@

frontendserver: frontendserver.o servercommon.o singleconnserverhtml.o \
	singleconnservercontrol.o cookierelay.o backendrelay.o
	$(CC) $^ $(CFLAGS) -o $@
	
singleconnserverhtml: singleconnserverhtml.o servercommon.o mailservice.o
	$(CC) $^ $(CFLAGS) -o $@
	
singleconnservercontrol: singleconnservercontrol.o servercommon.o
	$(CC) $^ $(CFLAGS) -o $@
	
cookierelay: cookierelay.o servercommon.o
	$(CC) $^ $(CFLAGS) -o $@
	
backendrelay: backendrelay.o servercommon.o
	$(CC) $^ $(CFLAGS) -o $@
	
mailservice: mailservice.o
	$(CC) $^ $(CFLAGS) -o $@
	
#%.o: %.c 
#	$(CC) -c -o $@ $< $(CFLAGS)

#frontendserver: frontendserver.cc
#	g++ -pthread $< -o $@

pack:
	rm -f submit-finalproject.zip
	zip -r submit-finalproject.zip *.cc README Makefile

clean:
	rm -fv $(TARGETS) *.o
