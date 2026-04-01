/* 
 * Operating Systems  (2INC0)  Practical Assignment.
 * Condition Variables Application.
 *
 * Defne Uyguc (2078007)
 * Dora Er (2074478)
 * Tamer Yurtsever (2081350)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>  
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "prodcons.h"

static ITEM buffer[BUFFER_SIZE];

static void rsleep (int t);
static ITEM get_next_item (void);

// mutex + condition variables
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv_consumer = PTHREAD_COND_INITIALIZER;
static pthread_cond_t producer_cvs[NROF_PRODUCERS];

// buffer + ordering state
static int buf_head = 0;
static int buf_tail = 0;
static int buf_count = 0;
static ITEM next_produce = 0;
static int items_consumed = 0;

// track which producers are waiting and for what item
static bool producer_waiting[NROF_PRODUCERS];
static ITEM waiting_item[NROF_PRODUCERS];

// counters for advanced feature
static unsigned long signal_count = 0;
static unsigned long broadcast_count = 0;

// signal consumer
static void signal_consumer(void)
{
    pthread_cond_signal(&cv_consumer);
    signal_count++;
}

// signal one producer
static void signal_producer(int id)
{
    pthread_cond_signal(&producer_cvs[id]);
    signal_count++;
}

// wake only the producer that has the next required item
static void signal_interested_producer_if_possible(void)
{
    if (buf_count == BUFFER_SIZE)
        return;

    for (int i = 0; i < NROF_PRODUCERS; i++)
    {
        if (producer_waiting[i] && waiting_item[i] == next_produce)
        {
            signal_producer(i);
            return;
        }
    }
}


/* producer thread */
static void * 
producer (void * arg)
{
    int id = *((int *) arg);

    while (true /* TODO: not all items produced */)
    {
        // TODO: 
        // * get the new item
        ITEM item = get_next_item();

        if (item == NROF_ITEMS)
            break;

        rsleep (100);	// simulating all kind of activities...
		
	// TODO:
	      // * put the item into buffer[]
	//
        // follow this pseudocode (according to the ConditionSynchronization lecture):
        //      mutex-lock;
        //      while not condition-for-this-producer
        //          wait-cv;
        //      critical-section;
        //      possible-cv-signals;
        //      mutex-unlock;

        pthread_mutex_lock(&mutex);

        // wait until it's this item's turn and buffer has space
        while (item != next_produce || buf_count == BUFFER_SIZE)
        {
            producer_waiting[id] = true;
            waiting_item[id] = item;
            pthread_cond_wait(&producer_cvs[id], &mutex);
        }

        producer_waiting[id] = false;

        // insert item into circular buffer
        buffer[buf_head] = item;
        buf_head = (buf_head + 1) % BUFFER_SIZE;
        buf_count++;
        next_produce++;

        signal_consumer(); // notify consumer
        signal_interested_producer_if_possible(); // wake correct producer

        pthread_mutex_unlock(&mutex);
    }
	return (NULL);
}

/* consumer thread */
static void * 
consumer (void * arg)
{
    (void) arg;

    while (true /* TODO: not all items retrieved from buffer[] */)
    {
        // TODO: 
	      // * get the next item from buffer[]
	      // * print the number to stdout
        //
        // follow this pseudocode (according to the ConditionSynchronization lecture):
        //      mutex-lock;
        //      while not condition-for-this-consumer
        //          wait-cv;
        //      critical-section;
        //      possible-cv-signals;
        //      mutex-unlock;

        pthread_mutex_lock(&mutex);

        // wait while buffer empty
        while (buf_count == 0 && items_consumed < NROF_ITEMS)
        {
            pthread_cond_wait(&cv_consumer, &mutex);
        }

        if (items_consumed == NROF_ITEMS)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // remove item from buffer
        ITEM item = buffer[buf_tail];
        buf_tail = (buf_tail + 1) % BUFFER_SIZE;
        buf_count--;
        items_consumed++;

        signal_interested_producer_if_possible(); // maybe next producer can go

        pthread_mutex_unlock(&mutex);

        rsleep (100);		// simulating all kind of activities...

        printf("%d\n", item);
    }
	return (NULL);
}

int main (void)
{
    // TODO: 
    // * startup the producer threads and the consumer thread
    // * wait until all threads are finished  

    pthread_t producers[NROF_PRODUCERS];
    pthread_t cons;
    int producer_ids[NROF_PRODUCERS];

    for (int i = 0; i < NROF_PRODUCERS; i++)
    {
        pthread_cond_init(&producer_cvs[i], NULL);
        producer_waiting[i] = false;
        waiting_item[i] = NROF_ITEMS;
        producer_ids[i] = i;
    }

    pthread_create(&cons, NULL, consumer, NULL);

    for (int i = 0; i < NROF_PRODUCERS; i++)
    {
        pthread_create(&producers[i], NULL, producer, &producer_ids[i]);
    }

    for (int i = 0; i < NROF_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }

    pthread_mutex_lock(&mutex);
    signal_consumer(); // ensure consumer can exit
    pthread_mutex_unlock(&mutex);

    pthread_join(cons, NULL);

    for (int i = 0; i < NROF_PRODUCERS; i++)
    {
        pthread_cond_destroy(&producer_cvs[i]);
    }

    fprintf(stderr, "signals    : %lu\n", signal_count);
    fprintf(stderr, "broadcasts : %lu\n", broadcast_count);

    return (0);
}