.PHONY: all build clean
all: build
	./my-sum 8 5 input.txt output.txt
	cat output.txt

build: my-sum.cpp Makefile
	c++ -std=c++11 my-sum.cpp -o my-sum

clean:
	rm -f my-sum output.txt
