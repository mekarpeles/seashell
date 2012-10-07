seashell: seashell.o

seashell.o: seashell.c
	gcc -c seashell.c -w

clean: 
	rm seashell.o
