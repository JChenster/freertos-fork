#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "my_semaphore.h"
#include "my_queue.h"

#include <stdio.h>

// Identifiers for test
#define SIMPLE (0)
#define PRODUCER_CONSUMER_SIMPLE (1)

#define STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

// Set this to 1 to use MyQueue, else use default
#define USE_MY_QUEUE (0)
// Set this to what test you want to run
#define RUNNING_TEST (0)

// Macros to abstract away queue functions
#if USE_MY_QUEUE == 1
    #define QUEUE_NAME "My Queue"
    #define QUEUE_SEND_BACK(ITEM, DELAY) MyQueueSendToBack(MyQueue, \
                                                           ((void*) (ITEM)), \
                                                           (DELAY))
    #define QUEUE_RECEIVE(BUFFER, DELAY) MyQueueReceive(MyQueue, \
                                                        ((void*) (BUFFER)), \
                                                        (DELAY))
#else
    #define QUEUE_NAME "Default Queue"
    #define QUEUE_SEND_BACK(ITEM, DELAY) xQueueSendToBack(DefaultQueue, \
                                                          ((void*) (ITEM)), \
                                                          (DELAY))
    #define QUEUE_RECEIVE(BUFFER, DELAY) xQueueReceive(DefaultQueue, \
                                                       ((void*) (BUFFER)), \
                                                       (DELAY))
#endif

// Global queue variables
MyQueueHandle_t MyQueue;
QueueHandle_t DefaultQueue;

void InitializeQueue(UBaseType_t QueueLength, UBaseType_t ItemSize) {
    #if USE_MY_QUEUE == 1
        MyQueue = MyQueueCreate(QueueLength, ItemSize);
        configASSERT(MyQueue);
    #else
        DefaultQueue = xQueueCreate(QueueLength, ItemSize);
        configASSERT(DefaultQueue);
    #endif
}

// Test function declarations
void TestSimple();
void TestProducerConsumerSimple();

void main_my_queue(void) {
    printf("Using %s\n", QUEUE_NAME);

    #if RUNNING_TEST == SIMPLE
        printf("Running simple test\n");
        TestSimple();
    #elif RUNNING_TEST == PRODUCER_CONSUMER_SIMPLE
        printf("Running simple producer consumer test\n");
        TestProducerConsumerSimple();
    #else
        printf("Invalid test selection\n");
    #endif
}

void SimpleTaskFunc(void* parameters) {
    (void) parameters;

    InitializeQueue(8, sizeof(int));

    int x = 5;
    QUEUE_SEND_BACK(&x, portMAX_DELAY);
    printf("Sent %d\n", x);

    int y;
    QUEUE_RECEIVE(&y, portMAX_DELAY);
    printf("Received %d\n", y);

    vTaskDelete(NULL);
}

void TestSimple() {
    // send and receive one value
    xTaskCreate(SimpleTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    vTaskStartScheduler();
}

static void ProducerTaskFunc(void* Parameters) {
    (void) Parameters;
    int supply = 10;

    for (int i = 0; i < supply; ++i) {
        QUEUE_SEND_BACK(&i, portMAX_DELAY);
        printf("Sent %d\n", i);
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    vTaskDelete(NULL);
}

static void ConsumerTaskFunc(void* Parameters) {
    (void) Parameters;
    int reads = 10;

    for (int i = 0; i < reads; i++) {
        int value;
        QUEUE_RECEIVE(&value, portMAX_DELAY);
        printf("Received %d\n", value);
        vTaskDelay(pdMS_TO_TICKS(25));
    }

    vTaskDelete(NULL);
}

void TestProducerConsumerSimple() {
    // fast producer and slow consumer
    // should see values 0, 1, ..., 9 in order
    InitializeQueue(3, sizeof(int));

    xTaskCreate(ProducerTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
    xTaskCreate(ConsumerTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    vTaskStartScheduler();
}
