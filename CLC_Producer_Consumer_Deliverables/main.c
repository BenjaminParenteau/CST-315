#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

/*
Name: Benjamin Parenteau
Course: CST-315
Assignment: CLC Producer/Consumer Threads
Date: January 30, 2026

Summary:
Implements a producer and a consumer as separate pthreads. A 5 buffer
is shared between them using a simple full/empty flag and short sleeps to avoid
busy-spinning. This uses a mutex to .
*/

/* Shared 5 buffer and state flag. */

static int buffer[5];
static volatile int buffer_full[5] = {0, 0, 0, 0, 0};

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* Simple product generator and consumer printer. */
static int theProduct = 0;

int produce(void) {
    return theProduct++;
}

void consume() {
    fflush(stdout);
}

/* Put/get for a 5-word buffer using a flag and short sleeps. */
int put(int item) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < 5; i++) {
        if (buffer_full[i] == 0) {
            buffer[i] = item;
            buffer_full[i] = 1;
            printf("produced at slot: %d\n", i);
            usleep(500000);
            
        }
    }
    pthread_mutex_unlock(&lock);
    printf("buffer is full\n");
    usleep(500000);
    return -1;
}

int get(void) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < 5; i++) {
        if (buffer_full[i] == 1) {  
            buffer_full[i] = 0;
            printf("consumed at slot: %d\n", i);
            usleep(500000);
            
        }
    }
    pthread_mutex_unlock(&lock);
    printf("buffer is empty\n");
    usleep(500000);
    return -1;
}

void producer(void) {
    int i;
    while (1) {
        i = produce();
        i = put(i);
    }
}

void consumer(void) {
    while (1) {
        get();
        consume();
    }
}

static void *producer_thread(void *arg) {
    (void)arg;
    producer();
    return NULL;
}

static void *consumer_thread(void *arg) {
    (void)arg;
    consumer();
    return NULL;
}

int main(void) {
    printf("Producer and Consumer threads created\n");

    pthread_t prod_thread;
    pthread_t cons_thread;

    if (pthread_create(&prod_thread, NULL, producer_thread, NULL) != 0) {
        perror("pthread_create (producer)");
        return 1;
    }
    if (pthread_create(&cons_thread, NULL, consumer_thread, NULL) != 0) {
        perror("pthread_create (consumer)");
        return 1;
    }

    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);
    return 0;
}
