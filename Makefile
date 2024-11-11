makeT: Globals.cpp  pattern.cpp SO6.cpp Z2.cpp main.cpp
#	g++ main.cpp SO6.cpp Z2.cpp pattern.cpp -std=c++11 -pthread -O3 -o main.out -fopenmp -march=-march='znver2'
#	g++ test_so6.cpp SO6.cpp Z2.cpp pattern.cpp Globals.cpp -O0 -std=c++20 -o test.out -lboost_program_options -pg 
#	g++ test_Z2.cpp SO6.cpp Z2.cpp pattern.cpp Globals.cpp -std=c++11 -pthread -O3 -o main.out -fopenmp -lboost_program_options
	g++ main.cpp SO6.cpp Z2.cpp pattern.cpp Globals.cpp --std=c++20 -O3 -pthread -o main.out -fopenmp -lboost_program_options -funroll-loops -march=native -flto=auto -Ofast
#	g++ main.cpp SO6.cpp  Z2.cpp pattern.cpp Globals.cpp --std=c++20 -O0 -pthread -o main.out -fopenmp -lboost_program_options -pg

