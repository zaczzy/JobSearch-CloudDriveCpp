GCC=g++
CFLAGS=-Wall -g -lpthread -lboost_serialization  
OBJ = socket.o  key_value.o thread.o logging.o

storage_server: storage_server.cc $(OBJ)
	$(GCC) $^ $(CFLAGS) -o $@ 

%.o: %.c 
	$(CC) $(CFLAGS) -c -o $@ $< 

pack:
	rm -f submit-hw2.zip
	zip -r cis505-project.zip *.cc *.h README Makefile

clean::
	rm -fv $(TARGETS) *~ main *.o storage_server

realclean:: clean
	rm -fv cis505-project.zip
