TARGETS = lb fes mailclient storage_server storage_server

cloud:
	make -f Makefile_fes
	make -f Makefile_lb

fes:
	make -f Makefile_fes

lb:
	make -f Makefile_lb
	
nishanth:
	make -f Makefile_nishanth

ritika:
	make -f Makefile_ritika
	
clean:
	rm -fv $(TARGETS) *.o
