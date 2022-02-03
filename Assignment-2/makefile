CC = g++
FLAGS = -std=c++11 -O2

all: shell.o parser.o process.o multiwatch.o sighandlers.o
	$(CC) $(FLAGS) -o shell shell.o parser.o process.o multiwatch.o sighandlers.o
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

