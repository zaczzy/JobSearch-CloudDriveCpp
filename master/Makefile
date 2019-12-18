
CC_FLAGS = -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization  \
-Winit-self -Wlogical-op  -Wmissing-include-dirs -Wnoexcept  -Woverloaded-virtual \
-Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default \
-Wundef -Werror -Wno-unused #-sWold-style-cast -Wmissing-declarations -Wformat=2
# S_FLAGS = -fsanitize=address -fno-omit-frame-pointer -pedantic
VALGRIND_DEBUG_FLAG = -g
GDB_DEBUG_FLAG = -ggdb
CC = g++ -std=c++11 $(CC_FLAGS) $(GDB_DEBUG_FLAG) $(VALGRIND_DEBUG_FLAG)
.DEFAULT_GOAL := all
all: master_server test_client

%.o: %.cc %.h
	$(CC) -o $@ -c $<

master_server: master_server.cc util.o thread_pool.o master_processor.o master_thread_handler.o server_config_store.o md5.o
	$(CC) $^ -lpthread -lssl -lcrypto -o $@ 

test_client: client.cc
	$(CC) $^ -o $@

.PHONY: clean
clean::
	rm -fv master_server test_client *~ *.o

realclean:: clean
	rm -fv cis505-hw2.zip

# how to run:
# ./master_server -t config.txt -p 10030
# ./test_client 10030
