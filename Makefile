makeT: SO6.cpp Z2.cpp main.cpp
#	g++ main.cpp SO6.cpp Z2.cpp pattern.cpp -std=c++11 -pthread -O3 -o main.out -fopenmp -march=-march='znver2'
	g++ main.cpp SO6.cpp Z2.cpp pattern.cpp -std=c++11 -pthread -O3 -o main.out -fopenmp -pg
