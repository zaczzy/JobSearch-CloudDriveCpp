TARGETS = echoserver mailclient
OBJS_POP3 = pop3_thread.o pop3.o
OBJS_SMTP = smtp_thread.o smtp.o

all: $(TARGETS)

echoserver: echothread.o echoserver.o
	gcc $^ -lpthread -o $@

echoserver.o: echoserver.c 
	gcc -c $^ -g -lpthread -o $@

echothread.o: echothread.c 
	gcc -c $^ -g -o $@

mailclient: mailclient.o mailservice.o
	gcc $^ -o $@

mailclient.o: mailclient.c
	gcc -c $^ -g -o $@

mailservice.o: mailservice.c
	gcc -c $^ -g -o $@

smtp: $(OBJS_SMTP)
	g++ $^ -lpthread -g -o $@
 
smtp.o: smtp.c 
	gcc -c $^ -g -lpthread -o $@

smtp_thread.o: smtp_thread.c 
	gcc -c $^ -g -o $@

pop3: $(OBJS_POP3)
	g++ $^ -lpthread -g -o $@

pop3.o: pop3.c 
	gcc -c $^ -g -lpthread -o $@

pop3_thread.o: pop3_thread.c 
	gcc -c $^ -g -o $@

pack:
	rm -f submit-hw2.zip
	zip -r submit-hw2.zip *.cc README Makefile

clean::
	rm -fv $(TARGETS) *~ *.o

realclean:: clean
	rm -fv cis505-hw2.zip
