CFLAGS=-Wall -Wextra -std=c99
build:
	gcc $(CFLAGS) -g -o vma main.c vma.c
run_vma:
	./vma
clean:
	rm -r vma