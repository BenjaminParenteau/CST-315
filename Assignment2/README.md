CST-315 Assignment 2 - Monitors and Semaphores

Build Instructions:
gcc -Wall -Wextra -O2 -pthread a2_semaphore.c -o a2_sem
gcc -Wall -Wextra -O2 -pthread a2_monitor.c -o a2_mon

Run:
./a2_sem
./a2_mon

Testing:
- Use various producer/consumer combinations (e.g., 2/2, 5/1, 1/5) (complete)
- Verify totals show produced = 1000 and consumed = 1000 (complete)
