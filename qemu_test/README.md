# Testing QEMU snapshot/restore

## Basic running sequence

- `./run`
- wait for boot (approx. 30 seconds)
- username: `root`, password: `alpine`
- run `./a.out` in qemu

## Run with lldb

- `./run 1`
- Type `r` at prompt
- wait for SIGUSR2
- copy command (starts with `process handle`...)
- run command
- continue (`c`)
- go to basic running sequence step 2

## General info

- to enter qemu monitor, do `c-a c`
    - type the same thing to exit
- to stop, do `c-a x`
- to take a snapshot, stop vm and do:
    - `migrate "exec:cat > migration_file_name"`
- to restore from a snapshot, stop vm and do:
    - `migrate_incoming "exec:cat migration_file_name"`
    - can run with `-incoming defer` and enter qemu monitor
- to run from a snapshot, add to qemu command line:
    - `-incoming "exec:cat migration_file_name"`

## Known issues

- on m1 mac, stepping in lldb after `__builtin_trap()` does not work


## creating ubuntu image

https://powersj.io/posts/ubuntu-qemu-cli/
- download ubuntu cloud image
- generate seed image with `cloud-localds seed.img user-data.yaml metadata.yaml`
- log in with ssh: `ssh -p 2222 ubuntu@0.0.0.0`

## connecting to tmux instance

``` sh
( exec </dev/tty; exec <&1; tmux -S /home/richyliu/qemu-fuzzing/none attach )
```

``` sh
➜  fuzzer_bridge git:(main) ✗ nc -U /tmp/qemu_qmp
➜  fuzzer_bridge git:(main) ✗ sudo chmod 777 /tmp/qemu_qmp

➜  fuzzer_bridge git:(main) ✗ make && scp -P 2222 test ubuntu@0.0.0.0:/home/ubuntu/
```

```
{"execute":"qmp_capabilities"}
{ "execute": "fast-snapshot-save", "arguments": { "filename": "/dev/shm/snapshot0" } }
{ "execute": "fast-snapshot-load", "arguments": { "filename": "/dev/shm/snapshot0" } }

{"execute":"stop"}
{"execute":"cont"}
```
