GCC=g++
CPPFLAGS=-Wall -g -lpthread
OBJ = frontendserver.o servercommon.o singleconnserverhtml.o \
singleconnservercontrol.o cookierelay.o backendrelay.o mailservice.o #loadbalancer.o

frontendserver: $(OBJ)
	$(GCC) $^ $(CFLAGS) -o $@
	
%.o: %.c 
	$(GCC) -c -o $@ $< $(CFLAGS)
 

TARGETS = frontendserver
#
#all: $(TARGETS)
#
#frontendserver: frontendserver.cc
#	g++ -pthread $< -o $@

pack:
	rm -f submit-finalproject.zip
	zip -r submit-finalproject.zip *.cc README Makefile

clean:
	rm -fv $(TARGETS) *.o
