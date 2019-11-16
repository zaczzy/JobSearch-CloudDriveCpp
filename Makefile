GCC=g++
CFLAGS=-Wall -lpthread -g

storage_server: storage_server.cc socket.cc thread.cc key_value.cc
	$(GCC) $^ $(CFLAGS) -o $@

pack:
	rm -f submit-hw2.zip
	zip -r cis505-project.zip *.cc *.h README Makefile

clean::
	rm -fv $(TARGETS) *~ main

realclean:: clean
	rm -fv cis505-project.zip
