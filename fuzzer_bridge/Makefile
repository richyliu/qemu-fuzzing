CC := /opt/homebrew/opt/llvm/bin/clang
LIBFUZZER_NO_MAIN := /usr/lib/llvm-12/lib/clang/12.0.0/lib/linux/libclang_rt.fuzzer_no_main-x86_64.a

UNAME := $(shell uname -s)
ifeq ($(UNAME), Linux)
  CC := clang-12
endif

all: test server_fuzz reset_mem

libclient.a: CFLAGS := -g -O0
libclient.a: client.o coverage.o
	$(AR) -rcs $@ $^

# makefile of program being fuzzed

# to integrate the client fuzzing harness, only two changes need to be made:
# 1. Change -fsanitize=fuzzer to -fsanitize=fuzzer-no-link. This builds with the
#    instrumentation only, without the fuzzing code and entrypoint.
# 2. Add the static library libclient.a when linking the final executable (the
#    one containing the LLVMFuzzerTestOneInput function).

test: CFLAGS := -g -O0 -fsanitize=fuzzer-no-link
test: LDFLAGS += -fsanitize=fuzzer-no-link
# to integrate with client.so: build with fuzzer-no-link instead of fuzzer
# and statically link with libclient.a for the main function
test: test.o libclient.a
	$(CC) $(LDFLAGS) $(DO_STATIC) -o $@ $^

# end of makefile of program being fuzzed

server_fuzz: CFLAGS := -g -O0
server_fuzz: LDFLAGS := -lpthread -lm
server_fuzz: server.o $(LIBFUZZER_NO_MAIN)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^

# server: CFLAGS := -g -O0
# server: server.o

reset_mem: reset_mem.o

clean:
	rm -f test test.o
	rm -f client.o coverage.o libclient.a
	rm -f server server.o server_fuzz
	rm -f reset_mem reset_mem.o

fresh: clean all
