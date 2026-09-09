.PHONY: all check asan clean

all:
	$(MAKE) -C src all
	$(MAKE) -C test all
	$(MAKE) -C doc/examples all

check: all
	mkdir -p build
	./test/out/test_wise
	./src/out/wise_combine_test doc/examples/connection.ct doc/examples/functions.ct \
		--lib doc/examples/libconn.so --report text > build/example-report.txt
	./src/out/wise_combine_test doc/examples/connection.ct doc/examples/functions.ct \
		--mode standalone --report text

asan:
	$(MAKE) -C src asan
	$(MAKE) -C test asan
	ASAN_OPTIONS=detect_leaks=0 ./test/out/test_wise_asan

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean
	$(MAKE) -C doc/examples clean
	rm -rf build
