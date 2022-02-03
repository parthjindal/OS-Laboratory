shell: shell.h shell.cc parser.o process.o multiwatch.o sighandlers.o
	g++ -o shell shell.cc parser.o process.o multiwatch.o sighandlers.o
clean:
	rm -f shell parser.o process.o multiwatch.o

parser.o: parser.cc parser.h
	g++ -c parser.cc -o parser.o

process.o: process.cc process.h
	g++ -c process.cc -o process.o

sighandlers.o: sighandlers.cc sighandlers.h
	g++ -c sighandlers.cc -o sighandlers.o

multiwatch.o: multiwatch.cc multiwatch.h
	g++ -c multiwatch.cc -o multiwatch.o

