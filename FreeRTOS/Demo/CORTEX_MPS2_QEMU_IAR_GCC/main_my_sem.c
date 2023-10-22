/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "my_semaphore.h"

/* Standard includes. */
#include <stdio.h>

#define BLOCK_TICKS pdMS_TO_TICKS(50UL)
#define ITERATIONS (3)
#define STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
#define SEM_WAIT_TICKS (BLOCK_TICKS * 4)
#define TIMER_TICKS pdMS_TO_TICKS(500UL)

#define NOT_REQUIRED (0)
#define REQUIRED (1)

// Identifiers for test
#define BINARY_SAME_PRIORITY (0)
#define BINARY_DIFF_PRIORITY (1)
#define COUNTING_SAME_PRIORITY (2)
#define COUNTING_DIFF_PRIORITY (3)
#define GIVE_SAME_PRIORITY (4)
#define GIVE_DIFF_PRIORITY (5)
#define INTERRUPTS (6)

// Set this to 1 to use MySemaphore, else use defualt
#define USE_MY_SEM (0)
// Set this to what test you want to run
#define RUNNING_TEST (0)

#define IS_BINARY_TEST (RUNNING_TEST == BINARY_SAME_PRIORITY || \
                        RUNNING_TEST == BINARY_DIFF_PRIORITY || \
                        RUNNING_TEST == GIVE_SAME_PRIORITY || \
                        RUNNING_TEST == GIVE_DIFF_PRIORITY)

// Semaphores
MySemaphoreHandle_t MySemaphore;
SemaphoreHandle_t xSemaphore = NULL;

// Task level attributes
#define MAX_TASKS (5)
int Priorities[MAX_TASKS];

typedef struct _task_info_t {
    char require_take;
    char require_give;
} task_info_t;

task_info_t TaskInfos[MAX_TASKS];

// For static task creation
StaticTask_t TaskBuffer;
StackType_t Stack[STACK_SIZE];
TaskHandle_t task_handle;

// Macros to take and give semaphore
#if (USE_MY_SEM == 1)
    #define SEM_NAME "My Semaphore"
    #define SEM_TAKE() MySemaphoreTake(MySemaphore, SEM_WAIT_TICKS)
    #define SEM_GIVE() MySemaphoreGive(MySemaphore)
#else
    #define SEM_NAME "Default Semaphore"
    #define SEM_TAKE() xSemaphoreTake(xSemaphore, SEM_WAIT_TICKS)
    #define SEM_GIVE() xSemaphoreGive(xSemaphore)
#endif
// Test function declarations
void TestBinarySamePriority();
void TestBinaryDiffPriority();
void TestCountingSamePriority();
void TestCountingDiffPriority();
void TestGiveSamePriority();
void TestGiveDiffPriority();
void TestInterrupts();

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
    #elif RUNNING_TEST == GIVE_SAME_PRIORITY
        printf("Running give semaphore test with same priority tasks\n");
        TestGiveSamePriority();
    #elif RUNNING_TEST == GIVE_DIFF_PRIORITY
        printf("Running give semaphore test with different priority tasks\n");
        TestGiveDiffPriority();
    #elif RUNNING_TEST == INTERRUPTS
        printf("Running interrupts test\n");
        TestInterrupts();
    #else
        printf("Invalid RUNNING_TEST\n");
    #endif
}

static void SemTaskFunc(void* pvParamaters) {
    int task_num = (int) pvParamaters;
    TickType_t xNextWakeTime = xTaskGetTickCount();

    // sleep for a little so that smaller number tasks go first
    vTaskDelayUntil(&xNextWakeTime, pdMS_TO_TICKS(5UL * (task_num + 1)));

    // default binary semaphore starts with a task needing to give the semaphore
    if (USE_MY_SEM != 1 && task_num == 0 && IS_BINARY_TEST) {
        SEM_GIVE();
    }

    // each process aims to take the sem a certain number of times
    for (int takes = 0; takes < ITERATIONS;) {
        // take semaphore
        BaseType_t taken = SEM_TAKE();
        if (TaskInfos[task_num].require_take == REQUIRED) {
            configASSERT(taken == pdTRUE);
        }

        if (taken == pdTRUE) {
            printf("Task %d TAKE SUCCESS\n", task_num);
            ++takes;
        }

        // give the semaphore
        if (taken == pdTRUE) {
            // sleep and hold the semaphore
            vTaskDelay(BLOCK_TICKS);

            printf("Task %d GIVE\n", task_num);
            BaseType_t given = SEM_GIVE();
            if (TaskInfos[task_num].require_give == REQUIRED) {
                configASSERT(given == pdTRUE);
            }
        }

        // sleep before trying to take again so that a waiting process can take
        // the semaphore. but sleep less than BLOCK_TICKS so that it is ready
        // after the next taker is done
        vTaskDelay(BLOCK_TICKS / 2);
    }

    // delete itself
    vTaskDelete(NULL);
}

static void GiveTaskFunc(void* pvParamaters) {
    int task_num = (int) pvParamaters;
    TickType_t xNextWakeTime = xTaskGetTickCount();

    // sleep for a little so that smaller number tasks go first
    vTaskDelayUntil(&xNextWakeTime, pdMS_TO_TICKS(5UL * (task_num + 1)));

    for (int i = 0; i < ITERATIONS; ++i) {
        if (task_num == 0) {
            // first process always take semaphore
            // gives some time for all processes to give
            vTaskDelay(2 * BLOCK_TICKS);

            // take for every process
            for (int j = 0; j < 3; ++j) {
                SEM_TAKE();
                printf("Task 0 TAKE\n");
            }
        } else {
            // while the rest of the processes wait to give
            SEM_GIVE();
            printf("Task %d GIVE\n", task_num);
            vTaskDelay(BLOCK_TICKS);
        }
    }

    vTaskDelete(NULL);
}

// helper function for testing binary semaphore
void TestBinary(int num_tasks, void task_func (void*)) {
    // create binary semaphore
    #if (USE_MY_SEM == 1)
        MySemaphore = MySemaphoreCreate(1, 1);
        configASSERT(MySemaphore);
    #else
        xSemaphore = xSemaphoreCreateBinary();
        configASSERT(xSemaphore);
    #endif

    for (int i = 0; i < num_tasks; ++i) {
        xTaskCreate(task_func,
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

    TestBinary(3, SemTaskFunc);
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

    TestBinary(3, SemTaskFunc);
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

// note that behavior of my semaphore and default semaphore differs here since
// default semaphore give is non-blocking, while my semaphore give is blocking
void TestGiveSamePriority() {
    // this test only makes sense for my semaphore
    configASSERT(USE_MY_SEM == 1);

    // task 0 takes sem
    // tasks 1, 2, 3 should give sem in that order
    for (int i = 0; i < 4; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1;
    }

    TestBinary(4, GiveTaskFunc);
}

void TestGiveDiffPriority() {
    // this test only makes sense for my semaphore
    configASSERT(USE_MY_SEM == 1);

    // task 0 takes sem
    // tasks 3, 2, 1 should give sem in that order
    for (int i = 0; i < 4; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1 + i;
    }

    TestBinary(4, GiveTaskFunc);
}

void InterruptTaskFunc(void* Parameters) {
    (void) Parameters;

    int interrupts = 0;

    for (int i = 0; i < 10; ++i) {
        // accept 3 values from timer and then take semaphore at which point
        uint32_t value = ulTaskNotifyTake(pdTRUE, TIMER_TICKS * 2);
        if (value > 0) {
            ++interrupts;
            printf("Timer ISR is sending %ld\n", value);

            if (interrupts == 3)  {
                taskENTER_CRITICAL();

                taskEXIT_CRITICAL();
//                SEM_TAKE();
            }
        } else {
            printf("No value received from Timer ISR\n");
        }
    }

    vTaskDelete(NULL);
}

void TimerCallbackFunc(TimerHandle_t Timer) {
    (void) Timer;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xTaskNotifyFromISR(task_handle,
                       429,
                       eSetValueWithOverwrite,
                       &xHigherPriorityTaskWoken);
}

void TestInterrupts() {
    // this test only makes sense for my semaphore
    configASSERT(USE_MY_SEM == 1);

    MySemaphore = MySemaphoreCreate(5, 3);
    configASSERT(MySemaphore);

    TimerHandle_t xTimer = xTimerCreate(NULL,
                                        TIMER_TICKS, // timer period
                                        pdTRUE, // reload
                                        NULL,
                                        TimerCallbackFunc);
    xTimerStart(xTimer, 0);

    task_handle = xTaskCreateStatic(InterruptTaskFunc,
                                    NULL,
                                    STACK_SIZE,
                                    NULL,
                                    tskIDLE_PRIORITY + 1,
                                    Stack,
                                    &TaskBuffer);

    vTaskStartScheduler();
}
