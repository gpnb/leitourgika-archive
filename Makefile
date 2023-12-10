all: a b

a: proccess_a.c
	gcc -g -Wall proccess_a.c -o proca -pthread -lrt

b: proccess_b.c
	gcc -g -Wall proccess_b.c -o procb -pthread -lrt

clean:
	rm -f proca procb