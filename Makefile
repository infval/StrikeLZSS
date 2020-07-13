.PHONY: all release debug
all:
	gcc -O3 -Wall -Wextra -static -static-libgcc main.c -o StrikeLZSS.exe
release:
	gcc -O3 -Wall -Wextra -static -static-libgcc main.c -o StrikeLZSS.exe -DNDEBUG
debug:
	gcc -g3 -Wall -Wextra -static -static-libgcc main.c -o StrikeLZSS.exe
