CPP=g++ 
INCLUDE=-I./include -I/usr/include/mysql
LIBS=-L/usr/lib/mysql -lmysqlclient -lpthread
CPPFLAGS=-g -Wall -Wno-write-strings $(INCLUDE)
PROGRAM=Server

SRC=src/main.cc\
	src/server.cc\

OBJ=$(SRC:.cc=.o)

all : $(PROGRAM)

%.o: src/%.cc include/%.h
	$(CPP) $(CPPFLAGS) $< -o $@

$(PROGRAM): $(OBJ) 
	$(CPP) $(CPPFLAGS) $(LIBS) -o $@ $(OBJ)

clean:
	@echo "Removing objects..."
	rm -f $(PROGRAM) *.log src/*.o src/*~ include/*~ *~ core

ar:
	make clean
	tar -czvf ../$(PROGRAM)"(`date`)".tar.gz *
