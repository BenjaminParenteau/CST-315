# Implementation approach and reasoning

This solution models the producer/consumer problem using two POSIX threads (`pthread`): one producer thread repeatedly generates an integer “product”, and one consumer thread repeatedly removes an integer product and prints it.

A fixed-size 5-slot buffer is represented as an `int buffer[5]` plus a per-slot flag array `buffer_full[5]` where `0` means empty and `1` means full. A single `pthread_mutex_t` protects all shared state (`buffer` and `buffer_full`) so that the producer and consumer cannot modify the same slot concurrently.

`put()` scans for the first empty slot and writes the item; if no empty slot exists, it reports “buffer is full”. `get()` scans for the first full slot and reads the item; if no full slot exists, it reports “buffer is empty”. After each operation a short `usleep()` is used to slow the output down and avoid a tight busy-spin in this simple demonstration (a more robust solution would typically use condition variables or semaphores to block instead of sleeping).

Build and run are done with `gcc` and the pthread library (`-pthread`).
