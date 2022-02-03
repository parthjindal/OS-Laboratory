CC = g++
FLAGS = -Wall -Wextra -std=c++11 -O2

all: shell.h shell.cc parser.o process.o multiwatch.o sighandlers.o
	$(CC) $(FLAGS) -o shell shell.cc parser.o process.o multiwatch.o sighandlers.o

clean:
	rm -f parser.o process.o multiwatch.o sighandlers.o

parser.o: parser.cc parser.h
	$(CC) $(FLAGS) -c parser.cc -o parser.o

process.o: process.cc process.h
	$(CC) $(FLAGS)  -c process.cc -o process.o

sighandlers.o: sighandlers.cc sighandlers.h
	$(CC) $(FLAGS) -c sighandlers.cc -o sighandlers.o

multiwatch.o: multiwatch.cc multiwatch.h
	$(CC) $(FLAGS) -c multiwatch.cc -o multiwatch.o

