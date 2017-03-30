proj1: main.o
	mpic++  -g -std=c++0x main.o -o proj2
	rm *.o

main.o: main.cpp
	mpic++ -I /usr/include/boost/ -g -std=c++0x -c main.cpp

test:
	make clean && make && mpirun -np 4 proj2 sr max D

clean:
	rm proj2 || true

