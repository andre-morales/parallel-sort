.PHONY: default build generate verify

default: build

build:
	gcc src/psort.c src/merge_sort.c src/selection_sort.c src/cond_lock.c -o ep -Wall -Werror -pthread -std=c11 -O -pg -Wno-unused-variable -Wno-unused-value

build-debug:
	gcc src/psort.c src/merge_sort.c src/selection_sort.c src/cond_lock.c -o epd -O0 -g -pg -fno-inline

generate:
	gcc ep_input_generator.c -o generate -O2 -fno-inline -pg

verify:
	gcc verify.c -o verify -O2