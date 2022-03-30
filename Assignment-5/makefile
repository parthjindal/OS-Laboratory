all: demo1 demo2 demo3

demo1: demo1.o libmemlab.a
	g++ -Wall -O2 demo1.o -lmemlab -L. -lpthread -o demo1

demo2: demo2.o libmemlab.a
	g++ -Wall -O2 demo2.o -lmemlab -L. -lpthread -o demo2

demo3: demo3.o libmemlab.a
	g++ -Wall -O2 demo3.o -lmemlab -L. -lpthread -o demo3

demo1.o: demo1.cc
	g++ -Wall -O2 -c demo1.cc

demo2.o: demo2.cc
	g++ -Wall -O2 -c demo2.cc

demo3.o: demo3.cc
	g++ -Wall -O2 -c demo3.cc

libmemlab.a: memlab.o
	ar -rcs libmemlab.a memlab.o	

memlab.o: memlab.cc memlab.h debug.h
	g++ -Wall -O2 -c memlab.cc

clean:
	rm -f demo1 demo2 demo3 demo1.o demo2.o demo3.o libmemlab.a memlab.o

