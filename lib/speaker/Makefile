.PHONY: speaker clean

LIB_DIR := lib/Linux-$(shell uname -m)
VERSION := $(cat VERSION)

speaker:
	mkdir -p build
	cd build && cmake ../src -DCMAKE_BUILD_TYPE=Release && make

clean:
	rm -rf build/ dist/
