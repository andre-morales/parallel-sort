.PHONY: default build generate verify test speed

SOURCES=src/psort.c src/barrier.c src/slow_barrier.c src/cond_lock.c src/allocator.c 

default: build

build: ep

build-debug:
	gcc $(SOURCES) -o epd -O -g -fno-inline -DUSE_SPINLOCKS=1

ep: Makefile src/*.c src/*.h
	gcc $(SOURCES) -o ep -Wall -Werror -pthread -std=gnu11 -O -Wno-unused-variable -Wno-unused-value -Wno-unused-function

generate:
	gcc ep_input_generator.c -o generate -O2 -fno-inline -pg

verify:
	gcc verify.c -o verify -O2

test: build
	./ep test/trouble.dat result.out 1
	./verify test/trouble.dat
	
	./ep test/cool.dat result.out 1
	./verify test/cool.dat
	
	./ep test/1kb.dat result.out 1
	./verify test/1kb.dat q
	
	./ep test/1mb.dat result.out 1
	./verify test/1mb.dat q
	
	./ep test/1mb.dat result.out 4
	./verify test/1mb.dat q
	
	./ep test/1mb.dat result.out 16
	./verify test/1mb.dat q
	
	./ep test/1mb.dat result.out 5
	./verify test/1mb.dat q

clean:
	rm result.out
	rm ep.exe
	rm ep