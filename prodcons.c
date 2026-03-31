/* 
 * Operating Systems  (2INC0)  Practical Assignment.
 * Condition Variables Application.
 *
 * STUDENT_NAME_1 (STUDENT_NR_1)
 * STUDENT_NAME_2 (STUDENT_NR_2)
 *
 * Grading:
 * Students who hand in clean code that fully satisfies the minimum requirements will get an 8. 
 * Extra steps can lead to higher marks because we want students to take the initiative.
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

static void rsleep (int t);         // already implemented (see below)
static ITEM get_next_item (void);   // already implemented (see below)

/* 
 * Synchronization variables
 *
 * One mutex protects all shared state.
 * cv_consumer: the consumer waits here when the buffer is empty.
 * producer_cvs[i]: producer i waits here when the buffer is full OR it is not
 *                  yet this producer's item's turn (item != next_produce).
 * next_produce:   the item number that must enter the buffer next (ascending order).
 * buf_count:      number of items currently in the circular buffer.
 * buf_head:       write index into buffer[].
 * buf_tail:       read  index into buffer[].
 * items_consumed: total items consumed so far (used to terminate consumer).
 *
 * Optimization:
 * Instead of broadcasting to all producers, each producer has its own condition
 * variable. This allows us to wake only the producer that is actually interested:
 * the one currently holding item == next_produce, provided the buffer is not full.
 */
static pthread_mutex_t mutex       = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv_consumer = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  producer_cvs[NROF_PRODUCERS];

static int  buf_head       = 0;
static int  buf_tail       = 0;
static int  buf_count      = 0;
static ITEM next_produce   = 0;
static int  items_consumed = 0;

/* 
 * For each producer we keep track of whether it is currently waiting and,
 * if so, which item it is holding. This lets us signal only the interested
 * producer instead of broadcasting to all producers.
 */
static bool producer_waiting[NROF_PRODUCERS];
static ITEM waiting_item[NROF_PRODUCERS];

/*
 * Counters required for the advanced feature.
 * They are printed to stderr at the end.
 */
static unsigned long signal_count    = 0;
static unsigned long broadcast_count = 0;

/*
 * Helper: signal the consumer and count it.
 */
static void
signal_consumer (void)
{
    pthread_cond_signal (&cv_consumer);
    signal_count++;
}

/*
 * Helper: signal producer 'id' and count it.
 */
static void
signal_producer (int id)
{
    pthread_cond_signal (&producer_cvs[id]);
    signal_count++;
}

/*
 * Helper:
 * If buffer is not full, find the producer that is currently waiting with
 * the next required item and wake only that producer.
 *
 * Must be called with mutex locked.
 */
static void
signal_interested_producer_if_possible (void)
{
    if (buf_count == BUFFER_SIZE)
    {
        return;
    }

    for (int i = 0; i < NROF_PRODUCERS; i++)
    {
        if (producer_waiting[i] && waiting_item[i] == next_produce)
        {
            signal_producer(i);
            return;     // exactly one producer can hold next_produce
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

        // sentinel: all items have been issued to producers
        if (item == NROF_ITEMS)
        {
            break;
        }
		
        rsleep (100);   // simulating all kind of activities...
		
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
        //
        // (see condition_test() in condition_basics.c how to use condition variables)

        pthread_mutex_lock (&mutex);

        // wait until it is this item's turn AND there is room in the buffer
        while (item != next_produce || buf_count == BUFFER_SIZE)
        {
            producer_waiting[id] = true;
            waiting_item[id] = item;
            pthread_cond_wait (&producer_cvs[id], &mutex);
        }

        // no longer waiting: this producer may proceed now
        producer_waiting[id] = false;
        waiting_item[id] = NROF_ITEMS;

        // critical section: write item into circular buffer
        buffer[buf_head] = item;
        buf_head  = (buf_head + 1) % BUFFER_SIZE;
        buf_count++;
        next_produce++;

        // wake consumer: a new item is available
        signal_consumer();

        // if another producer is already waiting with the new next_produce item
        // and buffer still has room, wake exactly that producer
        signal_interested_producer_if_possible();

        pthread_mutex_unlock (&mutex);
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

        pthread_mutex_lock (&mutex);

        // wait until the buffer is not empty, unless all items are already consumed
        while (buf_count == 0 && items_consumed < NROF_ITEMS)
        {
            pthread_cond_wait (&cv_consumer, &mutex);
        }

        // exit condition: all items have been consumed
        if (items_consumed == NROF_ITEMS)
        {
            pthread_mutex_unlock (&mutex);
            break;
        }

        // critical section: read item from circular buffer
        ITEM item = buffer[buf_tail];
        buf_tail  = (buf_tail + 1) % BUFFER_SIZE;
        buf_count--;
        items_consumed++;

        // consuming frees one slot, so maybe the interested producer can proceed now
        signal_interested_producer_if_possible();

        pthread_mutex_unlock (&mutex);

        rsleep (100);   // simulating all kind of activities...

        printf ("%d\n", item);
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
        pthread_cond_init (&producer_cvs[i], NULL);
        producer_waiting[i] = false;
        waiting_item[i] = NROF_ITEMS;
        producer_ids[i] = i;
    }

    pthread_create (&cons, NULL, consumer, NULL);

    for (int i = 0; i < NROF_PRODUCERS; i++)
    {
        pthread_create (&producers[i], NULL, producer, &producer_ids[i]);
    }

    for (int i = 0; i < NROF_PRODUCERS; i++)
    {
        pthread_join (producers[i], NULL);
    }

    /*
     * At this point all producers are done. The consumer may still be waiting
     * if the last relevant signal happened before it went to sleep, so wake it
     * once more to let it re-check its termination condition.
     */
    pthread_mutex_lock (&mutex);
    signal_consumer();
    pthread_mutex_unlock (&mutex);

    pthread_join (cons, NULL);

    for (int i = 0; i < NROF_PRODUCERS; i++)
    {
        pthread_cond_destroy (&producer_cvs[i]);
    }

    /*
     * Required by the advanced feature: show the number of signal and broadcast
     * calls on stderr.
     */
    fprintf (stderr, "signals    : %lu\n", signal_count);
    fprintf (stderr, "broadcasts : %lu\n", broadcast_count);

    return (0);
}

/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void 
rsleep (int t)
{
    static bool first_call = true;
    
    if (first_call == true)
    {
        srandom (time(NULL));
        first_call = false;
    }
    usleep (random () % t);
}


/* 
 * get_next_item()
 *
 * description:
 *	thread-safe function to get a next job to be executed
 *	subsequent calls of get_next_item() yields the values 0..NROF_ITEMS-1 
 *	in arbitrary order 
 *	return value NROF_ITEMS indicates that all jobs have already been given
 * 
 * parameters:
 *	none
 *
 * return value:
 *	0..NROF_ITEMS-1: job number to be executed
 *	NROF_ITEMS:	 ready
 */
static ITEM
get_next_item(void)
{
    static pthread_mutex_t  job_mutex   = PTHREAD_MUTEX_INITIALIZER;
    static bool    jobs[NROF_ITEMS+1] = { false }; // keep track of issued jobs
    static int     counter = 0;    // seq.nr. of job to be handled
    ITEM           found;          // item to be returned
	
	/* avoid deadlock: when all producers are busy but none has the next expected item for the consumer 
	 * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
	 */
	pthread_mutex_lock (&job_mutex);

	counter++;
	if (counter > NROF_ITEMS)
	{
	    // we're ready
	    found = NROF_ITEMS;
	}
	else
	{
	    if (counter < NROF_PRODUCERS)
	    {
	        // for the first n-1 items: any job can be given
	        // e.g. "random() % NROF_ITEMS", but here we bias the lower items
	        found = (random() % (2*NROF_PRODUCERS)) % NROF_ITEMS;
	    }
	    else
	    {
	        // deadlock-avoidance: item 'counter - NROF_PRODUCERS' must be given now
	        found = counter - NROF_PRODUCERS;
	        if (jobs[found] == true)
	        {
	            // already handled, find a random one, with a bias for lower items
	            found = (counter + (random() % NROF_PRODUCERS)) % NROF_ITEMS;
	        }    
	    }
	    
	    // check if 'found' is really an unhandled item; 
	    // if not: find another one
	    if (jobs[found] == true)
	    {
	        // already handled, do linear search for the oldest
	        found = 0;
	        while (jobs[found] == true)
            {
                found++;
            }
	    }
	}
    	jobs[found] = true;
			
	pthread_mutex_unlock (&job_mutex);
	return (found);
}