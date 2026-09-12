.PHONY: all check standalone standalone-asan asan clean

all:
	$(MAKE) -C src all
	$(MAKE) -C test all
	$(MAKE) -C doc/examples all

check: all
	mkdir -p build
	./test/out/test_wise
	./src/out/wise_combine_test test/fixtures/valid.ct \
		--lib test/out/libtest.so --report text > build/example-report.txt
	./src/out/wise_combine_test test/fixtures/adapter.ct \
		--adapter test/fixtures/adapter_ok.sh --report text
	./src/out/wise_combine_test test/fixtures/valid.ct \
		--mode standalone --lib test/out/libtest.so --report text
	g++ -std=c++17 build/wise_standalone.cpp -Ltest/out -ltest \
		-Wl,-rpath,'$$ORIGIN/../test/out' -o build/wise_standalone
	./build/wise_standalone

standalone: all
	./src/out/wise_combine_test test/fixtures/valid.ct \
		--mode standalone --lib test/out/libtest.so --report text
	g++ -std=c++17 build/wise_standalone.cpp -Ltest/out -ltest \
		-Wl,-rpath,'$$ORIGIN/../test/out' -o build/wise_standalone

standalone-asan: all
	./src/out/wise_combine_test test/fixtures/valid.ct \
		--mode standalone --lib test/out/libtest.so --report text
	g++ -std=c++17 -O1 -g -fsanitize=address -fno-omit-frame-pointer \
		build/wise_standalone.cpp -Ltest/out -ltest \
		-Wl,-rpath,'$$ORIGIN/../test/out' -o build/wise_standalone_asan
	ASAN_OPTIONS=detect_leaks=1 ./build/wise_standalone_asan

asan:
	$(MAKE) -C src asan
	$(MAKE) -C test asan
	ASAN_OPTIONS=detect_leaks=0 ./test/out/test_wise_asan

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean
	$(MAKE) -C doc/examples clean
	rm -rf build
