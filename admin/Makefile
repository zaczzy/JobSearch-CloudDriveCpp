TARGETS = admin_console
CC_FLAGS = -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization  \
-Winit-self -Wlogical-op  -Wmissing-include-dirs -Wnoexcept  -Woverloaded-virtual \
-Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default \
-Wundef -Werror -Wno-unused #-sWold-style-cast -Wmissing-declarations -Wformat=2
# S_FLAGS = -fsanitize=address -fno-omit-frame-pointer -pedantic
VALGRIND_DEBUG_FLAG = -g
GDB_DEBUG_FLAG = -ggdb
CC = g++ -std=c++11 $(CC_FLAGS) $(GDB_DEBUG_FLAG) $(VALGRIND_DEBUG_FLAG)
.DEFAULT_GOAL := admin_console
all: $(TARGETS)

%.o: %.cc %.h
	$(CC) -o $@ -c $<

admin_console: main.cc server_config_store.o 
	$(CC) $^ -lpthread -o $@

.PHONY: clean
clean::
	rm -fv $(TARGETS) *~ *.o

realclean:: clean
	rm -fv cis505-hw2.zip