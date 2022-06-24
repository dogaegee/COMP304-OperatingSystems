#include "queue.c"
#include "pthread.h"
#include "string.h"
#include "sys/time.h"
#include <stdbool.h>

#define QUEUE_LIMIT 1000
#define SIMULATION_T 2
#define LAUNCH 0
#define ASSEMBLE 1
#define LAND 2
#define EMERGENCY 3
#define MAX_LAUNCH 2
#define MAX_ASSEMBLE 1
#define PRINT_LENGTH 1024

int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 40; // frequency of emergency
float p = 0.2;               // probability of a ground job (launch & assembly)
time_t simulation_started;
int job_id = 0; //global job id counter
int n = 0; //default printing starting time

void* LandingJob(void *arg); 
void* LaunchJob(void *arg);
void* EmergencyJob(void *arg); 
void* AssemblyJob(void *arg); 
void* ControlTower(void *arg); 
void PushJob(pthread_mutex_t mutex, int type, Queue *queue);
void PopJob(Queue *queue, int t, int pad);
void* RunPadA(void *arg);
void* RunPadB(void *arg);
char* print_queue(Queue* pQueue);
void* print_snapshot();

struct Queue *launch_queue;
struct Queue *land_queue;
struct Queue *assemble_queue;
struct Queue *emergency_queue;

pthread_mutex_t launch_mutex; 
pthread_mutex_t land_mutex;
pthread_mutex_t assemble_mutex;
pthread_mutex_t emergency_mutex;
pthread_mutex_t pad_emergency_mutex;
pthread_mutex_t pad_A_mutex;
pthread_mutex_t pad_B_mutex;
pthread_mutex_t pop_mutex;
pthread_mutex_t pad_land_mutex;
pthread_mutex_t print_mutex;

// pthread sleeper function
int pthread_sleep (int seconds)
{
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    struct timespec timetoexpire;
    if(pthread_mutex_init(&mutex,NULL))
    {
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL))
    {
        return -1;
    }
    struct timeval tp;
    //When to expire is an absolute time, so get the current time and add it to our delay time
    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;
    
    pthread_mutex_lock (&mutex);
    int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock (&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);
    
    //Upon successful completion, a value of zero shall be returned
    return res;
}

int main(int argc,char **argv){
    // -p (float) => sets p
    // -t (int) => simulation time in seconds
    // -s (int) => change the random seed
    for(int i=1; i<argc; i++){
        if(!strcmp(argv[i], "-p")) {p = atof(argv[++i]);}
        else if(!strcmp(argv[i], "-t")) {simulationTime = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-s"))  {seed = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-n"))  {n = atoi(argv[++i]);}
    }
    
    srand(seed); // feed the seed
    
    /* Queue usage example
        Queue *myQ = ConstructQueue(1000);
        Job j;
        j.ID = myID;
        j.type = 2;
        Enqueue(myQ, j);
        Job ret = Dequeue(myQ);
        DestructQueue(myQ);
    */

    // your code goes here
    FILE *fp = fopen("events.log", "wb");
    fprintf(fp, "EventID\tStatus\t\tRequest Time\tEnd Time\tTurnaround Time\tPad\n");
    fprintf(fp, "____________________________________________________________________________________________\n");
    fclose(fp);
    //We created different queues for all type of jobs.
    launch_queue = ConstructQueue(QUEUE_LIMIT);
    land_queue = ConstructQueue(QUEUE_LIMIT);
    assemble_queue = ConstructQueue(QUEUE_LIMIT);
    emergency_queue = ConstructQueue(QUEUE_LIMIT);
    //mutex initialization.
    pthread_mutex_init(&launch_mutex, NULL);
    pthread_mutex_init(&land_mutex, NULL);
    pthread_mutex_init(&assemble_mutex, NULL);
    pthread_mutex_init(&emergency_mutex, NULL);
    pthread_mutex_init(&pad_A_mutex, NULL);
    pthread_mutex_init(&pad_B_mutex, NULL);
    pthread_mutex_init(&pop_mutex, NULL);
    pthread_mutex_init(&pad_land_mutex, NULL);
    pthread_mutex_init(&pad_emergency_mutex, NULL);
    pthread_mutex_init(&print_mutex, NULL);
    //thread initialization
    pthread_t tower_thread;
    pthread_t launch_thread;
    pthread_t land_thread;
    pthread_t assemble_thread;
    pthread_t print_thread;
    simulation_started = time(NULL);
    pthread_create(&tower_thread, NULL, ControlTower, NULL); //The control tower thread.
    pthread_detach(tower_thread);
    pthread_create(&launch_thread, NULL, LaunchJob, NULL); //The thread of the launching at time zero.
    pthread_detach(launch_thread);
    pthread_create(&print_thread, NULL, print_snapshot, NULL); //The thread of queue printing.
    pthread_detach(print_thread);
    pthread_sleep(SIMULATION_T);
    
    while(1){
    	if((time(NULL)-simulation_started) >= simulationTime){ //It stops generating new jobs when a specific real-time comes.
    		break;
    	}
    	float random_probability = (float) rand() / (float) RAND_MAX;
    	if(random_probability <=  p/2){ //With p/2 probability, it creates a launching thread.
    	    	pthread_t launch_thread;
    		pthread_create(&launch_thread, NULL, LaunchJob, NULL);
    		pthread_detach(launch_thread);
    	} else if(random_probability > p/2 && random_probability <= p) { //With p/2 probability, it creates a assembling thread.
    	    	pthread_t assemble_thread;
    		pthread_create(&assemble_thread, NULL, AssemblyJob, NULL);
    		pthread_detach(assemble_thread);
    	} else if(random_probability > p){ //With 1-p probability, it creates a landing thread.
    		pthread_t land_thread;
    		pthread_create(&land_thread, NULL, LandingJob, NULL);
    		pthread_detach(land_thread);
    	} else {
    		printf("Probability error occured!\n");
    	}
    	if((time(NULL) - simulation_started != 0) && ((time(NULL) - simulation_started) % emergencyFrequency*SIMULATION_T == 0)){ //At every 40t seconds, it creates 2 emergency threads.
		pthread_t emergency_thread1;
		pthread_t emergency_thread2;
		pthread_create(&emergency_thread1, NULL, EmergencyJob, NULL);
		pthread_create(&emergency_thread2, NULL, EmergencyJob, NULL);
    		pthread_detach(emergency_thread1);
    		pthread_detach(emergency_thread2);
    	}
    	pthread_sleep(SIMULATION_T);
    }
    return 0;
}
//This function prints the landing, launching and assemblying queues in the wanted format.
void* print_snapshot(){
	while(1){
		if((time(NULL)-simulation_started) >= simulationTime){ //It stops printing when a specific real-time comes.
    			break;
    		}
		if(time(NULL) - simulation_started > n){
			char* print_str = (char *) malloc(PRINT_LENGTH*sizeof(char));
    			char* current_time_str = (char *) malloc(PRINT_LENGTH*sizeof(char));
    			long long current_time = (long long) (time(NULL) - simulation_started);
    			sprintf(current_time_str, "%llu", current_time);
    			strcpy(print_str, "At ");
    			strcat(print_str, current_time_str);
    			strcat(print_str, " sec landing :");
    			char* land_print = print_queue(land_queue);
    			strcat(print_str, land_print);
    			strcat(print_str, "At ");
    			strcat(print_str, current_time_str);
    			strcat(print_str, " sec launch :");
    			char* launch_print = print_queue(launch_queue);
    			strcat(print_str, launch_print);
    			strcat(print_str, "At ");
    			strcat(print_str, current_time_str);
    			strcat(print_str, " sec assemble :");
    			char* assemble_print = print_queue(assemble_queue);
    			strcat(print_str, assemble_print);
    			printf("%s\n", print_str);
    			free(land_print);
    			free(launch_print);
    			free(assemble_print);
    			pthread_sleep(1);
    		}
    	}
    	pthread_exit(0);
}
//This function prints only one queue.
char* print_queue(Queue* pQueue){
	int i = 0;
	struct Node_t *current_node = (struct Node_t *)malloc(sizeof(struct Node_t *));
	char* queue_print = (char *) malloc(PRINT_LENGTH*sizeof(char));
	strcpy(queue_print, " ");
	char* current_queue_print = (char *) malloc(PRINT_LENGTH*sizeof(char));		
	current_node = pQueue->head; 
	while(current_node != NULL){ //continue until we reach a null node.
		sprintf(current_queue_print, "%d", current_node->data.ID); //save the id of the job as a string.
		strcat(queue_print, current_queue_print); 
		if(i != pQueue->size -1){
			strcat(queue_print, ",");
		}
		i++;
		current_node = current_node->prev; //continue with the next node.
	}
	strcat(queue_print, "\n");
	free(current_node);
	free(current_queue_print);
	return queue_print; //This is the string representing the job ids inside the queue, separated by commas.
}

// the function that creates plane threads for landing
void* LandingJob(void *arg){
	PushJob(land_mutex, LAND, land_queue);
	
}

// the function that creates plane threads for departure
void* LaunchJob(void *arg){
	PushJob(launch_mutex, LAUNCH, launch_queue);
}

// the function that creates plane threads for emergency landing
void* EmergencyJob(void *arg){
	PushJob(emergency_mutex, EMERGENCY, emergency_queue);
}

// the function that creates plane threads for emergency landing
void* AssemblyJob(void *arg){
	PushJob(assemble_mutex, ASSEMBLE, assemble_queue);
}

// the function that controls the air traffic
void* ControlTower(void *arg){
	//We created threads for each of the pads which are running by checking each other's current situations by mutexes.
	pthread_t A_thread;
	pthread_t B_thread;
    	pthread_create(&A_thread, NULL, RunPadA, NULL);
    	pthread_detach(A_thread);
    	pthread_create(&B_thread, NULL, RunPadB, NULL);
    	pthread_detach(B_thread);
	pthread_exit(0);
}

void PushJob(pthread_mutex_t mutex, int type, Queue *queue) {
	//We locked and unlocked this mutex because there can be multiple same type of Jobs waiting to be pushed into the same queue.
	pthread_mutex_lock(&mutex); 
	Job *new_job = ConstructJob(); //This is the constructor which we implemented inside the queue.c
	//Assigns the request time to the job and push it to the corresponding queue.
	new_job->req_time = time(NULL)-simulation_started; 
	job_id++; 
	new_job->ID = job_id;
	new_job->type = type;
	Enqueue(queue, *new_job);
	free(new_job);
	pthread_mutex_unlock(&mutex);
	pthread_exit(0);	
}

void PopJob(Queue *queue, int t, int pad) {
  	char pad_name; 
  	char status;
  	//We locked and unlocked this mutex because there can be multiple same type of Jobs waiting to be popped from the same queue.
  	pthread_mutex_lock(&pop_mutex);
  	Job pop_job = Dequeue(queue);
  	pthread_mutex_unlock(&pop_mutex);
  	pthread_sleep(t); //This is the passing time when Job is operated.
  	//Saves the ending time, turn around time and pad information to the Job.
  	pop_job.end_time = time(NULL)-simulation_started;
  	pop_job.ta_time = pop_job.end_time - pop_job.req_time;
  	pop_job.pad = pad;
  	if(pad == 0){ //Converts pad representing int to char.
  		pad_name = 'A';
  	} else {
  		pad_name = 'B';
  	}
  	if(pop_job.type == 0){ //Converts type representing int to char.
  		status = 'D';
  	} else if(pop_job.type == 1) {
  		status = 'A';
  	} else if(pop_job.type == 2) {
  		status = 'L';
  	} else if(pop_job.type == 3) {
  		status = 'E';
  	}
  	//Logging
  	FILE *fp = fopen("events.log", "a"); 
  	fprintf(fp, "%d\t\t%c\t\t%llu\t\t%llu\t\t%llu\t\t\t%c\n", pop_job.ID, status, (long long)pop_job.req_time, (long long)pop_job.end_time, (long long)pop_job.ta_time, pad_name);
  	fclose(fp);
}

void* RunPadA(void *arg){
	int launch_counter = 0; //This variable counts the consecutive launching Jobs which were operated.
	while(1){
		if((time(NULL)-simulation_started) >= simulationTime){ //It stops when a specific real-time comes.
    			break;
    		}
    		pthread_mutex_lock(&pad_A_mutex);
    		if(emergency_queue->size > 0) { //Emergency Jobs have the higher priority.
    			if(emergency_queue->size == 1){ 
    			//If there is only one emergency job, we need to give priority to one of the pads. That is why, we used mutex.
    				pthread_mutex_lock(&pad_emergency_mutex);
    				if(emergency_queue->size > 0) { //If there is still an emergency after the other pad operated on the emergency queue.
    					PopJob(emergency_queue, SIMULATION_T, 0);
    					launch_counter = 0;
				}
				//If the other pad operated on the one remaining emergency job, it does not do anything.
				pthread_mutex_unlock(&pad_emergency_mutex);
    			} else {
    				PopJob(emergency_queue, SIMULATION_T, 0); //Multiple emergencies, directly pop it.
    				launch_counter = 0;
    			}
    		} else if(launch_queue->size >= 3 && launch_counter < MAX_LAUNCH) { //To prevent starvation
    			PopJob(launch_queue, 2*SIMULATION_T, 0);
    			launch_counter++;
    		} else if(land_queue->size > 0){ 
			if(land_queue->size == 1){
			//If there is only one landing job, we need to give priority to one of the pads. That is why, we used mutex.
				pthread_mutex_lock(&pad_land_mutex);
				if(land_queue->size > 0){ //If there is still an landing after the other pad operated on the landing queue.
					PopJob(land_queue, SIMULATION_T, 0);
					launch_counter = 0;
				}
				//If the other pad operated on the one remaining landing job, it does not do anything.
				pthread_mutex_unlock(&pad_land_mutex);
			} else {
				PopJob(land_queue, SIMULATION_T, 0);
				launch_counter = 0;
			}
		} else if(launch_queue->size > 0){
			PopJob(launch_queue, 2*SIMULATION_T, 0);
			launch_counter++;
		}
		pthread_mutex_unlock(&pad_A_mutex);		
	}
	pthread_exit(0);
}
void* RunPadB(void *arg){
	int assemble_counter = 0;
	while(1){
		if((time(NULL)-simulation_started) >= simulationTime){ //It stops when a specific real-time comes.
    			break;
    		}
    		pthread_mutex_lock(&pad_B_mutex);
    		if(emergency_queue->size > 0) { //Emergency Jobs have the higher priority.
    			if(emergency_queue->size == 1){
    			//If there is only one emergency job, we need to give priority to one of the pads. That is why, we used mutex.
    				pthread_mutex_lock(&pad_emergency_mutex);
    				if(emergency_queue->size > 0) { //If there is still an emergency after the other pad operated on the emergency queue.
    					PopJob(emergency_queue, SIMULATION_T, 1);
    					assemble_counter = 0;
				}
				//If the other pad operated on the one remaining emergency job, it does not do anything.
				pthread_mutex_unlock(&pad_emergency_mutex);
    			} else {
    				PopJob(emergency_queue, SIMULATION_T, 1); //Multiple emergencies, directly pop it.
    				assemble_counter = 0;
    			}
    		} else if(assemble_queue->size > 3 && assemble_counter < MAX_ASSEMBLE) { //To prevent starvation
    			PopJob(assemble_queue, 6*SIMULATION_T, 1);
    			assemble_counter++;
    		} else if(land_queue->size > 0){
			if(land_queue->size == 1){
			//If there is only one landing job, we need to give priority to one of the pads. That is why, we used mutex.
				pthread_mutex_lock(&pad_land_mutex);
				if(land_queue->size > 0){ //If there is still an landing after the other pad operated on the landing queue.
					PopJob(land_queue, SIMULATION_T, 1);
					assemble_counter = 0;
				}
				//If the other pad operated on the one remaining landing job, it does not do anything.
				pthread_mutex_unlock(&pad_land_mutex);
			} else {
				PopJob(land_queue, SIMULATION_T, 1);
				assemble_counter = 0;
			}
		} else if(assemble_queue->size > 0){
			PopJob(assemble_queue, 6*SIMULATION_T, 1);
			assemble_counter++;
		}
		pthread_mutex_unlock(&pad_B_mutex);
	}
	pthread_exit(0);
}
