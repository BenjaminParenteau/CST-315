/*
 * File: a2_semaphore.c
 * Course: CST-315
 * Assignment 2: Monitors and Semaphores
 * Programmer: Benjamin Parenteau
 * Date: 2026-02-24
 *
 * Summary:
 * Simple Producer/Consumer using POSIX semaphores.
 * Demonstrates mutual exclusion and synchronization.
 *
 * Build:
 *   gcc -Wall -Wextra -O2 -pthread a2_semaphore.c -o a2_sem
 * Run:
 *   ./a2_sem
 */

#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>   // nanosleep

#define BUFSIZE 1000

static struct {
    int produced;
    int consumed;
    pthread_mutex_t mutex;
    sem_t empty;
    sem_t full;
} shared = {0}; // clean zero-init (avoids missing-field warning)

static void tiny_sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void *producer(void *arg) {
    int *count = (int *)arg;

    while (1) {
        // Reserve capacity; if none left, stop producing.
        if (sem_trywait(&shared.empty) != 0)
            return NULL;

        pthread_mutex_lock(&shared.mutex);
        shared.produced++;
        (*count)++;
        pthread_mutex_unlock(&shared.mutex);

        sem_post(&shared.full); // item available
        tiny_sleep_ms(1);
    }
}

static void *consumer(void *arg) {
    int *count = (int *)arg;

    while (1) {
        sem_wait(&shared.full); // wait for item

        pthread_mutex_lock(&shared.mutex);
        shared.consumed++;
        (*count)++;
        int done = (shared.consumed >= BUFSIZE);
        pthread_mutex_unlock(&shared.mutex);

        sem_post(&shared.empty); // space available
        tiny_sleep_ms(1);

        if (done) return NULL;
    }
}

int main(void) {
    int p, c;

    printf("Producers: ");
    if (scanf("%d", &p) != 1 || p <= 0) {
        fprintf(stderr, "Invalid producer count.\n");
        return 1;
    }

    printf("Consumers: ");
    if (scanf("%d", &c) != 1 || c <= 0) {
        fprintf(stderr, "Invalid consumer count.\n");
        return 1;
    }

    pthread_mutex_init(&shared.mutex, NULL);
    sem_init(&shared.empty, 0, BUFSIZE);
    sem_init(&shared.full, 0, 0);

    pthread_t *prod = calloc((size_t)p, sizeof(pthread_t));
    pthread_t *cons = calloc((size_t)c, sizeof(pthread_t));
    int *prodCount = calloc((size_t)p, sizeof(int));
    int *consCount = calloc((size_t)c, sizeof(int));
    if (!prod || !cons || !prodCount || !consCount) {
        fprintf(stderr, "Allocation failure.\n");
        return 1;
    }

    for (int i = 0; i < p; i++)
        pthread_create(&prod[i], NULL, producer, &prodCount[i]);

    for (int i = 0; i < c; i++)
        pthread_create(&cons[i], NULL, consumer, &consCount[i]);

    for (int i = 0; i < p; i++) {
        pthread_join(prod[i], NULL);
        printf("Producer %d produced %d\n", i, prodCount[i]);
    }

    for (int i = 0; i < c; i++) {
        pthread_join(cons[i], NULL);
        printf("Consumer %d consumed %d\n", i, consCount[i]);
    }

    printf("Total produced=%d consumed=%d\n", shared.produced, shared.consumed);

    sem_destroy(&shared.empty);
    sem_destroy(&shared.full);
    pthread_mutex_destroy(&shared.mutex);

    free(prod);
    free(cons);
    free(prodCount);
    free(consCount);
    return 0;
}