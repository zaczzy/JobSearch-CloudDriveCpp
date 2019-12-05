GCC=g++
CFLAGS=-Wall -g -lpthread #-lboost_serialization  
OBJ = storage_server.o socket.o  key_value.o thread.o

storage_server: $(OBJ)
	$(GCC) $^ $(CFLAGS) -o $@ 

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

pack:
	rm -f submit-hw2.zip
	zip -r cis505-project.zip *.cc *.h README Makefile

clean::
	rm -fv $(TARGETS) *~ main *.o storage_server

realclean:: clean
	rm -fv cis505-project.zip
