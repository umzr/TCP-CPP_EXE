CXX = g++
CXXFLAGS = -g -isystem -std=c++17 -pedantic -o2

all: wSender wReceiver

	
wSender: wSender.cpp
	$(CXX) $(CXXFLAGS) wSender.cpp -o wSender



	
wReceiver: wReceiver.cpp 
	$(CXX) $(CXXFLAGS) wReceiver.cpp -o wReceiver

clean:
	rm -rf wSender *.dSYM
	rm -rf wReceiver *.dSYM

.PHONY: clean
