/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "my_semaphore.h"

/* Standard includes. */
#include <stdio.h>

#define BLOCK_MS pdMS_TO_TICKS(1000UL)
#define STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
#define SEM_WAIT_MS pdMS_TO_TICKS(3500UL)

#define NOT_REQUIRED (0)
#define REQUIRED (1)

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
#define RUNNING_TEST (COUNTING_DIFF_PRIORITY)

MySemaphoreHandle_t MySemaphore;
SemaphoreHandle_t xSemaphore = NULL;

// Task level attributes
typedef struct _task_info_t {
    char require_take;
    char require_give;
} task_info_t;

#define MAX_TASKS (5)
int Priorities[MAX_TASKS];
task_info_t TaskInfos[MAX_TASKS];

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
    vTaskDelayUntil(&xNextWakeTime, pdMS_TO_TICKS(100UL * (task_num + 1)));

    for (;;) {
        // take semaphore
        BaseType_t taken = SEM_TAKE();
        if (TaskInfos[task_num].require_take == REQUIRED) {
            configASSERT(taken == pdTRUE);
        }
        if (taken) {
            printf("Task %d TAKE SUCCESS\n", task_num);
        }

        // give the semaphore
        if (taken) {
            // sleep and hold the semaphore
            vTaskDelay(BLOCK_MS);

            printf("Task %d GIVE\n", task_num);
            BaseType_t given = SEM_GIVE();
            if (TaskInfos[task_num].require_give == REQUIRED) {
                configASSERT(given == pdTRUE);
            }
        }

        // sleep before trying to take again so that a waiting process can take
        // the semaphore. but sleep less than BLOCK_MS so that it is ready
        // after the next taker is done
        vTaskDelay(BLOCK_MS / 2);
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
                    (void*) i,
                    Priorities[i], // assume that Priorities is configured
                    NULL);
    }

    vTaskStartScheduler();
}

void TestBinarySamePriority() {
    // all tasks should cycle through semaphore in the order of 0, 1, 2
    for (int i = 0; i < 3; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1;
        TaskInfos[i].require_give = REQUIRED;
        TaskInfos[i].require_take = REQUIRED;
    }

    TestBinary(3);
}

void TestBinaryDiffPriority() {
    // task 0 will take sem first
    // then afterwards, task 1 and 2 will share semaphore since they are
    // highest priority
    for (int i = 0; i < 3; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1 + i;
        TaskInfos[i].require_give = i == 0 ? NOT_REQUIRED : REQUIRED;
        TaskInfos[i].require_take = i == 0 ? NOT_REQUIRED : REQUIRED;
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
                    (void*) i,
                    Priorities[i], // assume that Priorities is configured
                    NULL);
    }

    vTaskStartScheduler();
}

void TestCountingSamePriority() {
    // all tasks should cycle through semaphore in the order of 0, 1, 2, 3, 4
    for (int i = 0; i < 5; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1;
        TaskInfos[i].require_give = REQUIRED;
        TaskInfos[i].require_take = REQUIRED;
    }

    TestCounting(5);
}

void TestCountingDiffPriority() {
    // task 0 will take sem first and then after that, it will be starved as
    // the other 4 sems share the 2 resources
    for (int i = 0; i < 5; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1 + i;
        TaskInfos[i].require_give = i == 0 ? NOT_REQUIRED : REQUIRED;
        TaskInfos[i].require_take = i == 0 ? NOT_REQUIRED : REQUIRED;
    }

    TestCounting(5);
}
