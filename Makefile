makeT: Globals.cpp  pattern.cpp SO6.cpp Z2.cpp main.cpp
	g++ main.cpp SO6.cpp Z2.cpp pattern.cpp Globals.cpp --std=c++20 -O3 -pthread -o main.out -fopenmp -lboost_program_options -funroll-loops -march=native -flto=auto -Ofast

