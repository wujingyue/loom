
CC = g++
CXX = g++
LEX = lex
YACC = yacc
AR = ar

FLAGS = -g

all:libparser.a

parser.y.c:parser.y
	$(YACC) -o parser.y.c -d parser.y

Decl.o:Decl.cpp
	$(CXX) $(FLAGS) -o $@ -c $^

Event.o:Event.cpp
	$(CXX) $(FLAGS) -o $@ -c $^

EventDesc.o:EventDesc.cpp
	$(CXX) $(FLAGS) -o $@ -c $^

EventNode.o:EventNode.cpp
	$(CXX) $(FLAGS) -o $@ -c $^

FixEvent.o:FixEvent.cpp
	$(CXX) $(FLAGS) -o $@ -c $^

parser.l.c:parser.l parser.y.c
	$(LEX) -o parser.l.c parser.l

parser.l.o:parser.l.c
	$(CXX) $(FLAGS) -o $@ -c $^

parser.y.o:parser.y.c
	$(CXX) $(FLAGS) -o $@ -c $^

parser.o:parser.cpp
	$(CXX) $(FLAGS) -o $@ -c $^

libparser.a:Decl.o Event.o EventDesc.o EventNode.o FixEvent.o parser.l.o parser.y.o
	$(AR) -r $@ $^

parser: parser.o libparser.a
	$(CXX) $(FLAGS) -o $@ $^ -ll -ly


run: parser
	./parser fix_test.conf parser.out >/dev/null

clean:
	rm -f parser.y.c parser.y.h
	rm -f parser.l.c
	rm -f parser
	rm -f *.o
	rm -f libparser.a
