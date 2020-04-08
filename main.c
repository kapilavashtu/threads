#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#define MAX_THREADS 12
#define MSG_SIZE 256
#define MAX_MSG 128

#define CLEAR_SCREEN printf("\e[1;1H\e[2J")
#define SHIFT_OUT printf("==========================================================\n")

uint32_t action_counter[MAX_THREADS];
uint32_t rx_counter = 0;

typedef struct node {
    char message[MSG_SIZE];
    struct node *next;
    int pos;
    int val;
} node;

static node *head = NULL;

static pthread_t threads_pool[MAX_THREADS];
static pthread_t sender_thread;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sender_cond, reciever_cond;

void *sender(void *arg);
void *reciever(void *arg);

static void sigusr1_handler(int signum);

int list_init(void);
int pop(char *buf);
int push(char *data);


int main(int argc, char **argv)
{
    if(signal (SIGUSR1, sigusr1_handler) == SIG_ERR)
    {
        perror("inpossible handle signal!\n");
        exit(EXIT_FAILURE);
    }

    int status = 0;

    if(list_init() == -1)
    {
        perror("cannot allocate memory");
        exit(EXIT_FAILURE);
    } else printf("list init ok!\n");


    pthread_cond_init(&sender_cond, 0);
    pthread_cond_init(&reciever_cond, 0);

    //create reciver threads
    for(int i = 0; i < MAX_THREADS; ++i)
    {
        status = pthread_create(&threads_pool[i], NULL, reciever, &i);
        if(status != 0)
        {
            printf("pthread_create reciever return error %d\n", status);
            exit(-1);
        }
    }

    //create sender
    status = pthread_create(&sender_thread, NULL, sender, 0);
    if(status != 0)
    {
        printf("pthread_create sender return error %d\n", status);
        exit(-1);
    }

    for(int i = 0; i < MAX_THREADS; i++)
    {
        pthread_join(threads_pool[i], NULL);
    }

    pthread_join(sender_thread, NULL);

    pthread_cond_destroy(&reciever_cond);
    pthread_cond_destroy(&sender_cond);
    pthread_mutex_destroy(&mutex);

    exit(EXIT_SUCCESS);
}

void *reciever(void *arg)
{
    int thread_number = *(int*) arg;
    char t_buf[MSG_SIZE];

    printf(": start reciever thread %d\n", thread_number);

    for(;;)
    {
        pthread_mutex_lock(&mutex);
        //if queue is empty - waiting
        int x = pop(t_buf);
        if((x == 1) || x == -1)
        {
            //perror("reciever wait - list is empty");
            pthread_cond_wait(&reciever_cond, &mutex);
        } else
        {
            //perror("reciever getting data");
            printf("# thread %d  %s", thread_number, t_buf);
            action_counter[thread_number]++;
            rx_counter++;
        }

        pthread_cond_signal(&sender_cond);
        pthread_mutex_unlock(&mutex);
    }
}

void *sender(void *arg)
{
    char t_buff[MSG_SIZE];
    time_t t;
    uint32_t tx_counter = 0;
    list_init();

    printf(": start sender thread\n");

    for(;;)
    {
        //get random number for delay
        int delay = 1000 + (rand() % 1000000);
        usleep(delay);
        time(&t);

        pthread_mutex_lock(&mutex);
        snprintf(t_buff, MSG_SIZE, "%d message at %s", tx_counter, ctime(&t));

        if(push(t_buff) == -1)
        {
            //if no more place for message - waiting
            perror("no more place for message - waiting");
            pthread_cond_wait(&sender_cond, &mutex);
        }

        pthread_cond_signal(&reciever_cond);
        pthread_mutex_unlock(&mutex);

        tx_counter++;
        usleep(100000);
    }
}

static void sigusr1_handler(int signum)
{
    CLEAR_SCREEN;
    printf("stat: for all threads recived messages = %d \n", rx_counter);

    for(int i = 0; i < MAX_THREADS; i++)
    {
        printf("stat: for thread %d recived message = %d \n", i, action_counter[i]);

    }
    SHIFT_OUT;
}

int pop(char *buf)
{
    if(head->next == NULL)
        return 1;
    node *prev = (node*) malloc(sizeof(node));
    if(head == NULL || prev == NULL)
        return -1;

    prev = head;
    strncpy(buf, prev->message, MSG_SIZE);
    head = head->next;
    free(prev);
    return 0;
}

int push(char *data)
{
    node *temp = (node*) malloc(sizeof(node));
    if(temp == NULL)
        return -1;

    strncpy(temp->message, data, MSG_SIZE);

    temp->pos = head->pos;
    temp->next = head->next;

    head->pos = head->pos + 1;
    head->next = temp;
    return 0;
}

int list_init(void)
{
    head = (node*) malloc(sizeof(node));
    if(head == NULL)
        return -1;

    head->pos = 0;
    head->next = NULL;

    return 0;
}


//end

