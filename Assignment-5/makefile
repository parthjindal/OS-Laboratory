all: demo1 demo2 demo3
FLAGS = -O2

demo1: demo1.o libmemlab.a
	g++ $(FLAGS) demo1.o -lmemlab -L. -lpthread -o demo1

demo2: demo2.o libmemlab.a
	g++ $(FLAGS) demo2.o -lmemlab -L. -lpthread -o demo2

demo3: demo3.o libmemlab.a
	g++ $(FLAGS) demo3.o -lmemlab -L. -lpthread -o demo3

demo1.o: demo1.cc
	g++ $(FLAGS) -c demo1.cc

demo2.o: demo2.cc
	g++ $(FLAGS) -c demo2.cc

demo3.o: demo3.cc
	g++ $(FLAGS) -c demo3.cc

libmemlab.a: memlab.o medium_int.o
	ar -rcs libmemlab.a memlab.o medium_int.o
	
medium_int.o: medium_int.cc medium_int.h
	g++ $(FLAGS) -c medium_int.cc

memlab.o: memlab.cc memlab.h debug.h 
	g++ $(FLAGS) -c memlab.cc

clean:
	rm -f demo1 demo2 demo3 demo1.o demo2.o demo3.o libmemlab.a memlab.o medium_int.o

