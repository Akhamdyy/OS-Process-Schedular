build:
	gcc process_generator.c -o process_generator.out
	gcc clk.c -o clk.out
	gcc process.c -o process.out
	gcc test_generator.c -o test_generator.out
	gcc scheduler.c mmu.c -o scheduler.out -lm

clean:
	rm -f *.out processes.txt request_*.txt scheduler.log memory.log scheduler.perf

all: clean build

run:
	./process_generator.out

test: build
	./test_generator.out