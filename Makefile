CPP=g++ 
INCLUDE=-I./include -I/usr/include/mysql
LIBS=-L/usr/lib/mysql -lmysqlclient -lpthread -lbluetooth -lopenobex
CPPFLAGS=-g -O3 -Wall -Wno-write-strings $(INCLUDE)
PROGRAM=Client

SRC=src/main.cc\
	src/client.cc\
	src/obex_main.cc\
	src/obex_socket.cc\

OBJ=$(SRC:.cc=.o)

all : $(PROGRAM)

%.o: src/%.cc include/%.h
	$(CPP) $(CPPFLAGS) $< -o $@

$(PROGRAM): $(OBJ) 
	$(CPP) $(CPPFLAGS) $(LIBS) -o $@ $(OBJ)

clean:
	@echo "Removing objects..."
	rm -f $(PROGRAM) *.log src/*.o src/*~ include/*~ *~ core src/*.swp include/*.swp

cleanlog:
	sudo rm -rf log/*

ar:
	make clean
	tar -czvf ../$(PROGRAM)"(`date`)".tar.gz *

