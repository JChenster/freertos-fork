/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "my_semaphore.h"

/* Standard includes. */
#include <stdio.h>

#define BLOCK_MS pdMS_TO_TICKS(2000UL)
#define STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
#define SEM_WAIT_MS pdMS_TO_TICKS(3000UL)

// Identifiers for test
#define BINARY_SAME_PRIORITY (0)
#define BINARY_DIFF_PRIORITY (1)
#define COUNTING_SAME_PRIORITY (2)
#define COUNTING_DIFF_PRIORITY (3)

// Test function declarations
void TestBinarySamePriority();
void TestBinaryDiffPriority();
void TestCountingSamePriority();
void TestCountingDiffPriority();

// Set this to 1 to use MySemaphore, else use defualt
# define USE_MY_SEM (1)
// Set this to what test you want to run
#define RUNNING_TEST (BINARY_SAME_PRIORITY)

MySemaphoreHandle_t MySemaphore;
SemaphoreHandle_t xSemaphore = NULL;

int Priorities[5];

// Macros to take and give semaphore
#if (USE_MY_SEM == 1)
    #define SEM_NAME "My Semaphore"
    #define SEM_TAKE() MySemaphoreTake(MySemaphore)
    #define SEM_GIVE() MySemaphoreGive(MySemaphore)
#else
    #define SEM_NAME "Default Semaphore"
    #define SEM_TAKE() xSemaphoreTake(xSemaphore, SEM_WAIT_MS)
    #define SEM_GIVE() xSemaphoreGive(xSemaphore)
#endif

void main_my_sem(void) {
    printf("Using %s\n", SEM_NAME);

    #if RUNNING_TEST == BINARY_SAME_PRIORITY
        printf("Running binary semaphore test with same priority tasks\n");
        TestBinarySamePriority();
    #elif RUNNING_TEST == BINARY_DIFF_PRIORITY
        printf("Running binary semaphore test with different priority tasks\n");
        TestBinaryDiffPriority();
    #elif RUNNING_TEST == COUNTING_SAME_PRIORITY
        printf("Running counting semaphore test with same priority tasks\n");
        TestCountingSamePriority();
    #elif RUNNING_TEST == COUNTING_DIFF_PRIORITY
        printf("Running counting semaphore test with different priority tasks\n");
        TestCountingDiffPriority();
    #else
        printf("Invalid RUNNING_TEST\n");
    #endif
}

static void SemTaskFunc(void* pvParamaters) {
    int task_num = (int) pvParamaters;

    TickType_t xNextWakeTime = xTaskGetTickCount();

    // sleep for a little so that smaller number tasks go first
    vTaskDelayUntil(&xNextWakeTime, pdMS_TO_TICKS(100UL * task_num));

    for (;;) {
        // take semaphore
        BaseType_t taken = SEM_TAKE();
        configASSERT(taken == pdTRUE);
        printf("Task %d TAKE SUCCESS\n", task_num);

        // sleep and hold the semaphore
        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);

        // give the semaphore
        printf("Task %d GIVE\n", task_num);
        BaseType_t given = SEM_GIVE();
        configASSERT(given == pdTRUE);

        // sleep before trying to take again
        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);
    }
}

// helper function for testing binary semaphore
void TestBinary(int num_tasks) {
    // create binary semaphore
    #if (USE_MY_SEM == 1)
        MySemaphore = MySemaphoreCreate(1, 1);
        configASSERT(MySemaphore);
    #else
        vSemaphoreCreateBinary(xSemaphore);
        configASSERT(xSemaphore);
    #endif

    for (int i = 0; i < num_tasks; ++i) {
        xTaskCreate(SemTaskFunc,
                    NULL,
                    STACK_SIZE,
                    (void*) (i + 1),
                    Priorities[i], // assume that Priorities is configured
                    NULL);
    }

    vTaskStartScheduler();
}

void TestBinarySamePriority() {
    for (int i = 0; i < 3; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1;
    }

    TestBinary(3);
}

void TestBinaryDiffPriority() {
    for (int i = 0; i < 3; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1 + i;
    }

    TestBinary(3);
}

void TestCounting(int num_tasks) {
    UBaseType_t max_count = 2;
    UBaseType_t init_count = 2;

    // create countingsemaphore
    #if (USE_MY_SEM == 1)
        MySemaphore = MySemaphoreCreate(max_count, init_count);
        configASSERT(MySemaphore);
    #else
        xSemaphore = xSemaphoreCreateCounting(max_count, init_count);
        configASSERT(xSemaphore);
    #endif

    for (int i = 0; i < num_tasks; ++i) {
        xTaskCreate(SemTaskFunc,
                    NULL,
                    STACK_SIZE,
                    (void*) (i + 1),
                    Priorities[i], // assume that Priorities is configured
                    NULL);
    }

    vTaskStartScheduler();
}

void TestCountingSamePriority() {
    for (int i = 0; i < 3; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1;
    }

    TestCounting(3);
}

void TestCountingDiffPriority() {
    // priorities should not matter since tasks sleep for varying amounts and
    // the order they take semaphore is based on that
    Priorities[0] = tskIDLE_PRIORITY + 1;
    Priorities[1] = tskIDLE_PRIORITY + 3;
    Priorities[2] = tskIDLE_PRIORITY + 2;

    TestCounting(3);
}
