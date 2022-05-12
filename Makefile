make:
	gcc --std=c99 assignment-3.c -o smallsh
clean:
	rm smallsh
test:
	./p3testscript > mytestresults 2>&1
edit:
	vim assignment-3.c
