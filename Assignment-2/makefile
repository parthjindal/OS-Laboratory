CC = g++
FLAGS = -std=c++11 -O2 -g

all: shell.o parser.o process.o multiwatch.o sighandlers.o history_search.o autocomplete.o getinput.cc
	$(CC) $(FLAGS) -o shell shell.o parser.o process.o multiwatch.o sighandlers.o history_search.o autocomplete.o getinput.cc
clean:
	rm -f parser.o process.o multiwatch.o sighandlers.o shell.o

shell.o: shell.h shell.cc 
	$(CC) $(FLAGS) -c shell.cc

parser.o: parser.cc parser.h
	$(CC) $(FLAGS) -c parser.cc -o parser.o

process.o: process.cc process.h
	$(CC) $(FLAGS)  -c process.cc -o process.o

sighandlers.o: sighandlers.cc sighandlers.h
	$(CC) $(FLAGS) -c sighandlers.cc -o sighandlers.o

multiwatch.o: multiwatch.cc multiwatch.h
	$(CC) $(FLAGS) -c multiwatch.cc -o multiwatch.o

autocomplete.o: autocomplete.cc autocomplete.h
	$(CC) $(FLAGS) -c autocomplete.cc -o autocomplete.o

history_search.o: history_search.cc history_search.h
	$(CC) $(FLAGS) -c history_search.cc -o history_search.o