.PHONY: default build generate verify test

SOURCES=src/psort.c src/radix_sort.c src/cond_lock.c src/merge_sort.c src/selection_sort.c

default: build

build:
	gcc $(SOURCES) -o ep -Wall -Werror -pthread -std=c11 -O -pg -Wno-unused-variable -Wno-unused-value -Wno-unused-function

build-debug:
	gcc $(SOURCES) -o epd -O0 -g -pg -fno-inline

generate:
	gcc ep_input_generator.c -o generate -O2 -fno-inline -pg

verify:
	gcc verify.c -o verify -O2

test: build
	./ep 1kb.dat result.out 1
	./verify 1kb.dat q
	./ep 1mb.dat result.out 1
	./verify 1mb.dat q
	./ep 1mb.dat result.out 4
	./verify 1mb.dat q
	./ep 1mb.dat result.out 16
	./verify 1mb.dat q
	./ep 1mb.dat result.out 5
	./verify 1mb.dat q
