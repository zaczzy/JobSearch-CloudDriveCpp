GCC=g++
CFLAGS=-Wall -g -lpthread
OBJ = frontendserver.o servercommon.o singleconnserverhtml.o \
singleconnservercontrol.o cookierelay.o #loadbalancer.o

frontendserver: $(OBJ)
	$(GCC) $^ $(CFLAGS) -o $@
	
%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)
 

TARGETS = frontendserver
#
#all: $(TARGETS)
#
#frontendserver: frontendserver.cc
#	g++ -pthread $< -o $@

pack:
	rm -f submit-finalproject.zip
	zip -r submit-finalproject.zip *.cc README Makefile

clean::
	rm -fv $(TARGETS) *.o
