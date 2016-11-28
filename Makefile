all:
	bison -d -o src/parser.tab.c src/parser.y
	flex -o src/lex.yy.c src/lexer.l
	gcc -std=gnu11 -c src/*.c
	g++ -std=c++0x -c src/*.cpp
	g++ *.o -o prolog -ly -lfl -lreadline
	rm *.o
