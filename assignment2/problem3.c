#include <stdint.h> // uint32
#include <stdlib.h> // exit, perror
#include <unistd.h> // usleep
#include <stdio.h> // printf
#include <pthread.h> // pthread_*
#ifdef __APPLE__
	#include <dispatch/dispatch.h>
	typedef dispatch_semaphore_t psem_t;
#else
	#include <semaphore.h> // sem_*
	typedef sem_t psem_t;
#endif
void psem_init(psem_t *sem, uint32_t value);
void psem_wait(psem_t sem);
void psem_post(psem_t sem);

void lock();
void unlock();


// Make the program a bit slower
#define SLOWDOWN usleep(1000)

#define total_stock 1000
int stock = total_stock;
int sold = 0;


pthread_t tid[1024] = {0}; // Max number of threads
int thread_count = 0; // Total number of utilized threads


// Sell 1 stock
void *sell(void *vargp)
{
	// TODO : Add lock()/unlock() calls in this function in appropriate places.
	lock();
	SLOWDOWN;
	if (stock > 0)
	{
		SLOWDOWN;
		stock -= 1;
		SLOWDOWN;
		sold += 1;
		printf("Sold. ");
	}
	else
	{
   	printf("No more stocks available.\n");
	}
	unlock();
   return NULL;
}

// Calls func() in a new thread
void create_new_thread(void *(func)(void* vargp))
{
	pthread_create(&tid[thread_count++], NULL, func, NULL);
}

// TODO : define any global variables you need here:
psem_t my_mutex;
// This code should result in more sales than stocks available
// by abusing the race condition
void trigger_race_condition()
{
	// TODO : code here
	int i;
	for(i = 0; i < 256; i++){
		create_new_thread(sell);
		thread_count++;
	}
}

// Initialization code
void init()
{
	// TODO : code here
	sem_init(&my_mutex, 0, 1);
}
// Acquire a lock
void lock()
{
	// TODO : code here
	sem_wait(&my_mutex);
}

// Release an acquired lock
void unlock()
{
	// TODO : code here
	sem_post(&my_mutex);
}

int main()
{
	init();

	trigger_race_condition();

	// Wait for all threads to finish execution
	for (int i=0; i<thread_count; ++i)
		pthread_join(tid[i], NULL);

	printf("\n");
	if (stock + sold == total_stock)
		printf("All done. %d stock left and %d sold (total %d).\n", stock, sold, total_stock);
	else
		printf("What just happened?! We have %d stock left but sold %d (total %d)!\n",
			stock, sold, stock+sold);

	return 0;
}

// psem_* functions are sem_* alternatives compatible on both macOs and Linux.
#ifdef __APPLE__
void psem_init(psem_t *sem, uint32_t value) {
	*sem = dispatch_semaphore_create(value);
}
void psem_wait(psem_t sem) {
	dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
}
void psem_post(psem_t sem) {
	dispatch_semaphore_signal(sem);
}
#else
void psem_init(psem_t *sem, u_int32_t value) {
	sem_init(sem, 0, value);
}
void psem_wait(psem_t sem) {
	sem_wait(&sem);
}
void psem_post(psem_t sem) {
	sem_post(&sem);
}
#endif

