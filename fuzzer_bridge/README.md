
for local testing sockets:

``` sh
socat -d -d pty,raw,echo=0 UNIX-LISTEN:/tmp/my_snapshot
```

creates a pty (simulates guest) and a socket (simulates host)


# openssl fuzzing

compile openssl:

``` sh
CC=/opt/homebrew/opt/llvm/bin/clang ./config enable-fuzz-libfuzzer \
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

# find serial port on guest

list pci controllers: `lspci`
look for "Serial controller"
more info with `lspci -vs4` (replace `4` with actual pci number)
correlate IRQ number with result from `setserial -g /dev/ttyS*`

``` sh
sudo setserial -g /dev/ttyS* | grep "IRQ: $(lspci -v | grep -A3 "Serial controller" | grep IRQ | sed -e 's/.*IRQ \([0-9]\+\)$/\1/')" | cut -d, -f1
```
