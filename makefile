life: life.c
	cc -o life life.c -lncurses
run: life life.c
	./life

