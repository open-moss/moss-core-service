.PHONY: listener clean

listener:
	mkdir -p build
	cd build && cmake ../src -DCMAKE_BUILD_TYPE=Release && make $(MAKEFLAGS)

clean:
	rm -rf build/
