CXX = g++
all:
	$(CXX) -O3 -g -Wall -std=c++11 main.cpp psd.cpp
	$(CXX) -g -Wall -o rwtest -std=c++11 rwtest.cpp psd.cpp
