TARGETS = frontendserver

all: $(TARGETS)

frontendserver: frontendserver.cc
	g++ -pthread $< -o $@

pack:
	rm -f submit-finalproject.zip
	zip -r submit-finalproject.zip *.cc README Makefile

clean::
	rm -fv $(TARGETS)
