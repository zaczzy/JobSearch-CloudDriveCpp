TARGETS = mailclient storage_server
OBJS_POP3 = pop3_thread.o pop3.o
OBJS_SMTP = smtp_thread.o smtp.o
CC=g++
CFLAGS=-std=c++11 -lpthread

all: $(TARGETS)

storage_server: storage_server.cc socket.cc thread.cc key_value.cc
	$(CC) $^ $(CFLAGS) -o $@

echoserver: echothread.o echoserver.o
	$(CC) $^ -lpthread -o $@

echoserver.o: echoserver.c 
	$(CC) -c $^ -g -lpthread -o $@

echothread.o: echothread.c 
	$(CC) -c $^ -g -o $@

mailclient: mailclient.o mailservice.o
	$(CC) $^ -o $@

mailclient.o: mailclient.c
	$(CC) -c $^ -g -o $@

mailservice.o: mailservice.c
	$(CC) -c $^ -g -o $@

smtp: $(OBJS_SMTP)
	$(CC) $^ -lpthread -g -o $@
 
smtp.o: smtp.c 
	$(CC) -c $^ -g -lpthread -o $@

smtp_thread.o: smtp_thread.c 
	$(CC) -c $^ -g -o $@

pop3: $(OBJS_POP3)
	$(CC) $^ -lpthread -g -o $@

pop3.o: pop3.c 
	$(CC) -c $^ -g -lpthread -o $@

pop3_thread.o: pop3_thread.c 
	$(CC) -c $^ -g -o $@

pack:
	rm -f submit-hw2.zip
	zip -r submit-hw2.zip *.cc README Makefile

clean::
	rm -fv $(TARGETS) *~ *.o

realclean:: clean
	rm -fv cis505-hw2.zip
