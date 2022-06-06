profiler: profiler.c
	gcc profiler.c -o profiler -lpfm

main: main.c
	gcc main.c -o main -lpfm

