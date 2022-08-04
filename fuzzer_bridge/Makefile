CC := /opt/homebrew/opt/llvm/bin/clang

UNAME := $(shell uname -s)
ifeq ($(UNAME), Linux)
  DO_STATIC := -static
  CC := clang-12
endif

all: test server

libclient.a: CFLAGS := -g -O0
libclient.a: client.o serial.o
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

server: server.o

clean:
	rm -f test test.o
	rm -f serial.o client.o libclient.a
	rm -f server server.o

fresh: clean all
