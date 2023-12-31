#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "my_semaphore.h"
#include "my_queue.h"

#include "CMSDK_CM3.h"
#include "core_cm3.h"

#include <stdio.h>

// Identifiers for test
#define SIMPLE (0)
#define FAST_SLOW (1)
#define SLOW_FAST (2)
#define SEND_BACK_ISR (3)
#define RECEIVE_ISR (4)

#define SEND_IRQN (UARTRX1_IRQn)
#define RECEIVE_IRQN (UARTTX1_IRQn)

#define STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

// Set this to 1 to use MyQueue, else use default
#define USE_MY_QUEUE (0)
// Set this to what test you want to run
#define RUNNING_TEST (0)

// Macros to abstract away queue functions
#if USE_MY_QUEUE == 1
    #define QUEUE_NAME "My Queue"
    #define QUEUE_SEND_BACK(ITEM, DELAY) xMyQueueSendToBack(MyQueue, \
                                                            ((void*) (ITEM)), \
                                                            (DELAY))
    #define QUEUE_SEND_BACK_ISR(ITEM, WOKEN) \
        xMyQueueSendToBackFromISR(MyQueue, (void*) (ITEM), (BaseType_t*) (WOKEN))
    #define QUEUE_RECEIVE(BUFFER, DELAY) xMyQueueReceive(MyQueue, \
                                                         ((void*) (BUFFER)), \
                                                         (DELAY))
    #define QUEUE_RECEIVE_ISR(BUFFER, WOKEN) \
        xMyQueueReceiveFromISR(MyQueue, \
                               ((void*) (BUFFER)), \
                               (BaseType_t*) (WOKEN))
#else
    #define QUEUE_NAME "Default Queue"
    #define QUEUE_SEND_BACK(ITEM, DELAY) xQueueSendToBack(DefaultQueue, \
                                                          ((void*) (ITEM)), \
                                                          (DELAY))
    #define QUEUE_SEND_BACK_ISR(ITEM, WOKEN) \
        xQueueSendToBackFromISR(DefaultQueue, \
                                 (void*) (ITEM), \
                                 (BaseType_t*) (WOKEN))
    #define QUEUE_RECEIVE(BUFFER, DELAY) xQueueReceive(DefaultQueue, \
                                                       ((void*) (BUFFER)), \
                                                       (DELAY))
    #define QUEUE_RECEIVE_ISR(BUFFER, WOKEN) \
        xQueueReceiveFromISR(DefaultQueue, \
                             ((void*) (BUFFER)), \
                             (BaseType_t*) (WOKEN))
#endif

// Global queue variables
MyQueueHandle_t MyQueue;
QueueHandle_t DefaultQueue;

// Util functions
void InitializeQueue(UBaseType_t QueueLength, UBaseType_t ItemSize) {
    #if USE_MY_QUEUE == 1
        MyQueue = pxMyQueueCreate(QueueLength, ItemSize);
        configASSERT(MyQueue);
    #else
        DefaultQueue = xQueueCreate(QueueLength, ItemSize);
        configASSERT(DefaultQueue);
    #endif
}

extern void CallIRQN(IRQn_Type irqn, uint32_t Priority);

// Test function declarations
void TestSimple();
void TestFastSlow();
void TestSlowFast();
void TestSendToBackFromISR();
void TestReceiveFromISR();

void main_my_queue(void) {
    printf("Using %s\n", QUEUE_NAME);

    #if RUNNING_TEST == SIMPLE
        printf("Running simple test\n");
        TestSimple();
    #elif RUNNING_TEST == FAST_SLOW
        printf("Running fast slow producer consumer test\n");
        TestFastSlow();
    #elif RUNNING_TEST == SLOW_FAST
        printf("Running slow fast producer consumer test\n");
        TestSlowFast();
    #elif RUNNING_TEST == SEND_BACK_ISR
        printf("Running SendToBackFromISR test\n");
        TestSendToBackFromISR();
    #elif RUNNING_TEST == RECEIVE_ISR
        printf("Running ReceiveFromISR test\n");
        TestReceiveFromISR();
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
    int SleepMs = (int) Parameters;
    int supply = 10;

    for (int i = 0; i < supply; ++i) {
        QUEUE_SEND_BACK(&i, portMAX_DELAY);
        printf("Sent %d\n", i);
        vTaskDelay(pdMS_TO_TICKS(SleepMs));
    }

    vTaskDelete(NULL);
}

static void ConsumerTaskFunc(void* Parameters) {
    int SleepMs = (int) Parameters;
    int reads = 10;

    for (int i = 0; i < reads; i++) {
        int value;
        QUEUE_RECEIVE(&value, portMAX_DELAY);
        printf("Received %d\n", value);
        vTaskDelay(pdMS_TO_TICKS(SleepMs));
    }

    vTaskDelete(NULL);
}

void TestFastSlow() {
    // fast producer and slow consumer
    // should see values 0, 1, ..., 9 in order
    InitializeQueue(3, sizeof(int));

    xTaskCreate(ProducerTaskFunc,
                NULL,
                STACK_SIZE,
                (void*) 5,
                tskIDLE_PRIORITY + 1,
                NULL);
    xTaskCreate(ConsumerTaskFunc,
                NULL,
                STACK_SIZE,
                (void*) 25,
                tskIDLE_PRIORITY + 1,
                NULL);

    vTaskStartScheduler();
}

void TestSlowFast() {
    // slow producer and fast consumer
    // should see values 0, 1, ..., 9 in order
    InitializeQueue(3, sizeof(int));

    xTaskCreate(ProducerTaskFunc,
                NULL,
                STACK_SIZE,
                (void*) 25,
                tskIDLE_PRIORITY + 1,
                NULL);
    xTaskCreate(ConsumerTaskFunc,
                NULL,
                STACK_SIZE,
                (void*) 5,
                tskIDLE_PRIORITY + 1,
                NULL);

    vTaskStartScheduler();
}

// *****************************************************************************
// TestSendToBackFromISR
// *****************************************************************************
BaseType_t ExpectedSendFromISRReturn, ExpectedSendFromISRWoken;
int SendItem;

void QueueSendFromISRHandler() {
    printf("SendToBackFromISR\n");
    BaseType_t HigherPriorityTaskWoken = pdFALSE;

    BaseType_t ret = QUEUE_SEND_BACK_ISR(&SendItem, &HigherPriorityTaskWoken);
    configASSERT(ret == ExpectedSendFromISRReturn);
    configASSERT(HigherPriorityTaskWoken == ExpectedSendFromISRWoken);
}

static void SendFromISRHighTaskFunc(void* Parameters) {
    (void) Parameters;

    // queue is size 3
    // calling SendToBackFromISR 3x should succeed
    // then afterwards, should fail
    ExpectedSendFromISRReturn = pdTRUE;
    ExpectedSendFromISRWoken = pdFALSE;

    for (int i = 0; i < 3; ++i) {
        SendItem = 490 + i;
        CallIRQN(SEND_IRQN, 1);
    }

    ExpectedSendFromISRReturn = errQUEUE_FULL;
    for (int i = 0; i < 5; ++i) {
        CallIRQN(SEND_IRQN, 1);
    }

    // check that elements were pushed on
    int receive;
    for (int i = 0; i < 3; ++i) {
        printf("QueueReceive\n");
        configASSERT(QUEUE_RECEIVE(&receive, 0) == pdTRUE);
        configASSERT(receive == 490 + i);
    }

    // test HigherPriorityTaskWoken
    // try to receive from an empty queue. this blocks and low priority task is
    // scheduled in. the low priority task is then interrupted by SendFromISR
    // which should then wake this task
    printf("Waiting to RECEIVE\n");
    configASSERT(QUEUE_RECEIVE(&receive, pdMS_TO_TICKS(100)) == pdTRUE);
    configASSERT(receive == 12345);
    printf("RECEIVE done\n");

    vTaskDelete(NULL);
}

static void SendFromISRLowTaskFunc(void* Parameters) {
    (void) Parameters;

    SendItem = 12345;
    ExpectedSendFromISRReturn = pdTRUE;
    ExpectedSendFromISRWoken = pdTRUE;

    CallIRQN(SEND_IRQN, 1);
    vTaskDelete(NULL);
}

void TestSendToBackFromISR() {
    InitializeQueue(3, sizeof(int));

    xTaskCreate(SendFromISRHighTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
    xTaskCreate(SendFromISRLowTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    vTaskStartScheduler();
}

// *****************************************************************************
// TestReceiveFromISR
// *****************************************************************************
BaseType_t ExpectedReceiveFromISRReturn, ExpectedReceiveFromISRWoken;
int ExpectedReceiveFromISRItem;

void QueueReceiveFromISRHandler() {
    printf("ReceiveFromISR\n");
    BaseType_t HigherPriorityTaskWoken = pdFALSE;

    int receive;
    BaseType_t ret = QUEUE_RECEIVE_ISR(&receive, &HigherPriorityTaskWoken);
    configASSERT(ret == ExpectedReceiveFromISRReturn);
    configASSERT(HigherPriorityTaskWoken == ExpectedReceiveFromISRWoken);

    // if we received an item
    if (ret == pdTRUE)
        configASSERT(receive == ExpectedReceiveFromISRItem);
}

static void ReceiveFromISRHighTaskFunc(void* Parameters) {
    (void) Parameters;

    for (int i = 0; i < 3; ++i) {
        printf("QueueSendBack\n");
        int Send = 490 + i;
        configASSERT(QUEUE_SEND_BACK(&Send, 0) == pdTRUE);
    }

    // queue is size 3
    // calling ReceiveFromISR 3x should succeed
    // then afterwards, should fail
    ExpectedReceiveFromISRReturn = pdTRUE;
    ExpectedReceiveFromISRWoken = pdFALSE;

    for (int i = 0; i < 3; ++i) {
        ExpectedReceiveFromISRItem = 490 + i;
        CallIRQN(RECEIVE_IRQN, 1);
    }

    ExpectedReceiveFromISRReturn = errQUEUE_EMPTY;
    for (int i = 0; i < 5; ++i) {
        CallIRQN(RECEIVE_IRQN, 1);
    }

    // test HigherPriorityTaskWoken
    // try to send to a full queue. this blocks and low priority task is
    // scheduled in. the low priority task is then interrupted by ReceiveFromISR
    // which should then wake this task
    for (int i = 0; i < 3; ++i) {
        printf("QueueSendBack\n");
        int Send = 12345 + i;
        configASSERT(QUEUE_SEND_BACK(&Send, 0) == pdTRUE);
    }

    printf("Waiting to SEND\n");
    int Send = 12345 + 3;
    configASSERT(QUEUE_SEND_BACK(&Send, pdMS_TO_TICKS(100)) == pdTRUE);
    printf("SEND done\n");

    vTaskDelete(NULL);
}

static void ReceiveFromISRLowTaskFunc(void* Parameters) {
    (void) Parameters;

    ExpectedReceiveFromISRReturn = pdTRUE;
    ExpectedReceiveFromISRWoken = pdTRUE;
    ExpectedReceiveFromISRItem = 12345;

    CallIRQN(RECEIVE_IRQN, 1);
    vTaskDelete(NULL);
}

void TestReceiveFromISR() {
    InitializeQueue(3, sizeof(int));

    xTaskCreate(ReceiveFromISRHighTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
    xTaskCreate(ReceiveFromISRLowTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    vTaskStartScheduler();
}
