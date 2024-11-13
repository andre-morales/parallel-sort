.PHONY: default build generate verify test speed

SOURCES=src/psort.c src/radix_sort.c src/barrier.c src/count_barrier.c src/slow_barrier.c src/cond_lock.c src/merge_sort.c src/allocator.c

default: build

build: ep

build-debug:
	gcc $(SOURCES) -o epd -O2 -g -fno-inline

generate:
	gcc ep_input_generator.c -o generate -O2 -fno-inline -pg

verify:
	gcc verify.c -o verify -O2

test: build
	./ep simple.dat result.out 1
	./verify simple.dat
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

/mnt/tmpfs/a:
	sudo mount ramfs -t ramfs /mnt/tmpfs/
	sudo mkdir /mnt/tmpfs/a
	sudo chmod 777 /mnt/tmpfs/a

/mnt/tmpfs/a/100mb.dat: /mnt/tmpfs/a
	cp 100mb.dat /mnt/tmpfs/a

/mnt/tmpfs/a/200mb.dat: /mnt/tmpfs/a
	cp 200mb.dat /mnt/tmpfs/a

speed100: /mnt/tmpfs/a/100mb.dat ep
	hyperfine -m 30 --warmup 100 "./ep /mnt/tmpfs/a/100mb.dat /mnt/tmpfs/a/result.out 8"

speed200: /mnt/tmpfs/a/200mb.dat ep
	hyperfine -m 30 --warmup 100 "./ep /mnt/tmpfs/a/200mb.dat /mnt/tmpfs/a/result.out 8"

ep: Makefile src/*.c src/*.h
	gcc $(SOURCES) -o ep -Wall -Werror -pthread -std=c11 -O -Wno-unused-variable -Wno-unused-value -Wno-unused-function
