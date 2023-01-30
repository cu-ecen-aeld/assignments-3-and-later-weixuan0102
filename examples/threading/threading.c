#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_func_args->thread_complete_success = true;
    usleep(thread_func_args->wait_to_obtain_ms * 1000);
    DEBUG_LOG("lock");
    if(pthread_mutex_lock(thread_func_args->mutex) != 0){
	    ERROR_LOG("Can't lock mutex");
	    thread_func_args->thread_complete_success = false;
    }
    usleep(thread_func_args->wait_to_release_ms * 1000);
    if(pthread_mutex_unlock(thread_func_args->mutex) != 0){
	    thread_func_args->thread_complete_success = false;
	    ERROR_LOG("Can't unlock mutex");  
    }
    DEBUG_LOG("unlock");
	
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data* tData = malloc(sizeof(struct thread_data));
    if(tData == NULL){
	    ERROR_LOG("Memory allocation failure for creating data");	
	    return false;
    }
    /*
    if(pthread_mutex_init(mutex, NULL) != 0){
	    ERROR_LOG("mutex initialization failed.");
	    return false;
    }
    */
    tData->wait_to_obtain_ms = wait_to_obtain_ms;
    tData->wait_to_release_ms = wait_to_release_ms;
    tData->mutex = mutex;
    int rc = pthread_create(thread, NULL, threadfunc, tData); 
    if(rc != 0){
	    ERROR_LOG("Can't create thread");
	    return false;
    }

    return true;
}

