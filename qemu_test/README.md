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
