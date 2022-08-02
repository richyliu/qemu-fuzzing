
for local testing sockets:

``` sh
socat -d -d pty,raw,echo=0 UNIX-LISTEN:/tmp/foo
```

creates a pty (simulates guest) and a socket (simulates host)


# openssl fuzzing

compile openssl:

``` sh
CC=/opt/homebrew/Cellar/llvm@12/12.0.1_1/bin/clang ./config enable-fuzz-libfuzzer \
        --with-fuzzer-lib=../libclient.a \
        -DPEDANTIC enable-asan enable-ubsan no-shared \
        -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION \
        -fsanitize=fuzzer-no-link \
        enable-ec_nistp_64_gcc_128 -fno-sanitize=alignment \
        enable-weak-ssl-ciphers enable-rc5 enable-md2 \
        enable-ssl3 enable-ssl3-method enable-nextprotoneg \
        no-tests \
        --debug
```

then `make -j4`

run one of the targets in fuzz/
