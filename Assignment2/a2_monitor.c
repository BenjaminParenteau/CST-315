/*
 * File: a2_monitor.c
 * Course: CST-315
 * Assignment 2: Monitors and Semaphores
 * Programmer: Benjamin Parenteau
 * Date: 2026-02-24
 *
 * Summary:
 * Simple Producer/Consumer using mutex + condition variables (monitor pattern).
 *
 * Build:
 *   gcc -Wall -Wextra -O2 -pthread a2_monitor.c -o a2_mon
 * Run:
 *   ./a2_mon
 */

#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>   // nanosleep

#define BUFSIZE 1000

static struct {
    int produced;
    int consumed;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} shared = {0};

static void tiny_sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void *producer(void *arg) {
    int *count = (int *)arg;

    while (1) {
        pthread_mutex_lock(&shared.mutex);

        if (shared.produced >= BUFSIZE) {
            pthread_cond_broadcast(&shared.not_empty);
            pthread_mutex_unlock(&shared.mutex);
            return NULL;
        }

        shared.produced++;
        (*count)++;

        pthread_cond_signal(&shared.not_empty);
        pthread_mutex_unlock(&shared.mutex);

        tiny_sleep_ms(1);
    }
}

static void *consumer(void *arg) {
    int *count = (int *)arg;

    while (1) {
        pthread_mutex_lock(&shared.mutex);

        while (shared.consumed >= shared.produced && shared.produced < BUFSIZE)
            pthread_cond_wait(&shared.not_empty, &shared.mutex);

        if (shared.consumed >= BUFSIZE) {
            pthread_mutex_unlock(&shared.mutex);
            return NULL;
        }

        shared.consumed++;
        (*count)++;

        pthread_mutex_unlock(&shared.mutex);

        tiny_sleep_ms(1);

        if (shared.consumed >= BUFSIZE) return NULL;
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
    pthread_cond_init(&shared.not_empty, NULL);

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

    pthread_cond_destroy(&shared.not_empty);
    pthread_mutex_destroy(&shared.mutex);

    free(prod);
    free(cons);
    free(prodCount);
    free(consCount);
    return 0;
}