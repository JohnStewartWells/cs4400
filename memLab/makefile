a.out: tester.o string_set.o node.o
	g++ tester.o string_set.o node.o

tester.o: node.h string_set.h tester.cpp
	g++ -c tester.cpp

string_set.o: string_set.h node.h string_set.cpp
	g++ -c string_set.cpp

node.o: node.cpp node.h
	g++ -c node.cpp

clean:
	rm -f tester.o node.o string_set.o a.out
