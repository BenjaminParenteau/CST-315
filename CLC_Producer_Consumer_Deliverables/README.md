# CST-315 Producer/Consumer (pthreads)

## Build
```bash
gcc -Wall -Wextra -O2 -pthread main.c -o pc
```

## Run
```bash
./pc
```

This program runs indefinitely (producer/consumer loops). For a short demo run in a terminal:
```bash
timeout 3 ./pc
```
