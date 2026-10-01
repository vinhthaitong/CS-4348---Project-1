CXXFLAGS = -std=c++11 -O2 -Wall -Wextra

.PHONY: all build bonus clean
all: build
	./my-sum 8 5 input.txt output.txt
	cat output.txt

build: my-sum.cpp Makefile
	c++ $(CXXFLAGS) my-sum.cpp -o my-sum

# Builds the bonus source under the assignment's required executable name.
bonus: my-sum-bonus.cpp Makefile
	c++ $(CXXFLAGS) my-sum-bonus.cpp -o my-sum

clean:
	rm -f my-sum
