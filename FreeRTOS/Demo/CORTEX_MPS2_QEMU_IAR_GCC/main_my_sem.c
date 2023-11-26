/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "my_semaphore.h"

#include "CMSDK_CM3.h"

/* Standard includes. */
#include <stdio.h>

#define BLOCK_TICKS pdMS_TO_TICKS(50UL)
#define ITERATIONS (3)
#define STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
#define SEM_WAIT_TICKS (BLOCK_TICKS * 4)

#define NOT_REQUIRED (0)
#define REQUIRED (1)

// Identifiers for test
#define BINARY_SAME_PRIORITY (0)
#define BINARY_DIFF_PRIORITY (1)
#define COUNTING_SAME_PRIORITY (2)
#define COUNTING_DIFF_PRIORITY (3)
#define GIVE_SAME_PRIORITY (4)
#define GIVE_DIFF_PRIORITY (5)
#define TAKE_FROM_ISR (6)
#define GIVE_FROM_ISR (7)

// Set this to 1 to use MySemaphore, else use default
#define USE_MY_SEM (0)
// Set this to what test you want to run
#define RUNNING_TEST (0)

#define IS_BINARY_TEST (RUNNING_TEST == BINARY_SAME_PRIORITY || \
                        RUNNING_TEST == BINARY_DIFF_PRIORITY || \
                        RUNNING_TEST == GIVE_SAME_PRIORITY || \
                        RUNNING_TEST == GIVE_DIFF_PRIORITY)

#define GIVE_FROM_ISR_IRQN (UARTRX0_IRQn)
#define TAKE_FROM_ISR_IRQN (UARTTX0_IRQn)

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
    #define SEM_TAKE() xMySemaphoreTake(MySemaphore, SEM_WAIT_TICKS)
    #define SEM_GIVE() MySemaphoreGive(MySemaphore, portMAX_DELAY)
    #define SEM_TAKE_ISR(WOKEN) \
        MySemaphoreTakeFromISR(MySemaphore, (BaseType_t*) (WOKEN))
    #define SEM_GIVE_ISR(WOKEN) \
        MySemaphoreGiveFromISR(MySemaphore, (BaseType_t*) (WOKEN))
#else
    #define SEM_NAME "Default Semaphore"
    #define SEM_TAKE() xSemaphoreTake(xSemaphore, SEM_WAIT_TICKS)
    #define SEM_GIVE() xSemaphoreGive(xSemaphore)
    #define SEM_TAKE_ISR(WOKEN) \
        xSemaphoreTakeFromISR(xSemaphore, (BaseType_t*) (WOKEN))
    #define SEM_GIVE_ISR(WOKEN) \
        xSemaphoreGiveFromISR(xSemaphore, (BaseType_t*) (WOKEN))
#endif

// Test function declarations
void TestBinarySamePriority();
void TestBinaryDiffPriority();
void TestCountingSamePriority();
void TestCountingDiffPriority();
void TestGiveSamePriority();
void TestGiveDiffPriority();
void TestTakeFromISR();
void TestGiveFromISR();

// util functions
extern void CallIRQN(IRQn_Type irqn, uint32_t Priority);

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
    #elif RUNNING_TEST == TAKE_FROM_ISR
        printf("Running take from ISR test\n");
        TestTakeFromISR();
    #elif RUNNING_TEST == GIVE_FROM_ISR
        printf("Running give from ISR test\n");
        TestGiveFromISR();
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
        MySemaphore = pxMySemaphoreCreate(1, 1);
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

    // create counting semaphore
    #if (USE_MY_SEM == 1)
        MySemaphore = pxMySemaphoreCreate(max_count, init_count);
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
    // expect tasks 1, 2, 3 should give sem in that order
    for (int i = 0; i < 4; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1;
    }

    TestBinary(4, GiveTaskFunc);
}

void TestGiveDiffPriority() {
    // this test only makes sense for my semaphore
    configASSERT(USE_MY_SEM == 1);

    // task 0 takes sem
    // expect tasks 3, 2, 1 should give sem in that order
    for (int i = 0; i < 4; ++i) {
        Priorities[i] = tskIDLE_PRIORITY + 1 + i;
    }

    TestBinary(4, GiveTaskFunc);
}

// *****************************************************************************
// TestTakeFromISR
// *****************************************************************************
int TakeFromISRTestCalls = 0;

// handler for TAKE_FROM_ISR_IRQN
void SemTakeFromISRHandler() {
    printf("TakeFromISR\n");
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t taken = SEM_TAKE_ISR(&xHigherPriorityTaskWoken);

    if (TakeFromISRTestCalls == 0) {
        configASSERT(taken == pdTRUE);
        configASSERT(xHigherPriorityTaskWoken == pdFALSE);
    } else if (TakeFromISRTestCalls == 1) {
        configASSERT(taken == pdFALSE);
        configASSERT(xHigherPriorityTaskWoken == pdFALSE);
    } else if (TakeFromISRTestCalls == 2) {
        configASSERT(taken == pdTRUE);
        configASSERT(xHigherPriorityTaskWoken == pdTRUE);
    }

    ++TakeFromISRTestCalls;
}

// indicates if low priority task should call TakeFromISR
int CallInteruptTakeFromISR = 0;

void TakeFromISRHighPriorityTaskFunc(void* pvParamaters) {
    (void) pvParamaters;

    // semaphore has initial count of 1 and max count of 2
    // summon interrupt to call TakeFromISR twice
    // first takeFromISR should succeed, second should fail
    CallIRQN(TAKE_FROM_ISR_IRQN, 1);
    CallIRQN(TAKE_FROM_ISR_IRQN, 1);

    // count of semaphore should be 0 so 2 givesshould be successful
    printf("GIVE\n");
    configASSERT(SEM_GIVE() == pdTRUE);
    printf("GIVE\n");
    configASSERT(SEM_GIVE() == pdTRUE);

    // semaphore is now full so we must wait now for TakeFromISR to give the
    // sempahore. when we wait to give, lower priority will go and we should
    // indicate to it to call the TakeFromISR interrupt
    CallInteruptTakeFromISR = 1;
    printf("Waiting to GIVE\n");
    configASSERT(SEM_GIVE() == pdTRUE);
    printf("Done GIVE\n");

    vTaskDelete(NULL);
}

void TakeFromISRLowPriorityTaskFunc(void* pvParamaters) {
    (void) pvParamaters;

    for (;;) {
        if (CallInteruptTakeFromISR) {
            CallIRQN(TAKE_FROM_ISR_IRQN, 1);
            vTaskDelete(NULL);
        }
    }
}

void TestTakeFromISR() {
    // Test the following
    // 1) When count > 0, success
    // 2) When count == 0, failure
    // 3) xHigherPriorityTaskWoken should be set appropriately
    // This test only makes sense for MySemaphore since default semaphore does
    // not block when calling Give which means TakeFromISR cannot awake any
    // waiting giver
    // https://stackoverflow.com/questions/69814969/freertos-how-could-xsemaphoretakefromisr-wake-any-task
    configASSERT(USE_MY_SEM == 1);

    // create counting semaphore
    UBaseType_t max_count = 2;
    UBaseType_t init_count = 1;

    #if (USE_MY_SEM == 1)
        MySemaphore = pxMySemaphoreCreate(max_count, init_count);
        configASSERT(MySemaphore);
    #else
        xSemaphore = xSemaphoreCreateCounting(max_count, init_count);
        configASSERT(xSemaphore);
    #endif

    xTaskCreate(TakeFromISRHighPriorityTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
    xTaskCreate(TakeFromISRLowPriorityTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    vTaskStartScheduler();
}

// ******************************************************************************
// TestGiveFromISR
// ******************************************************************************
int GiveFromISRTestCalls = 0;

// handler for GIVE_FROM_ISR_IRQN
void SemGiveFromISRHandler() {
    printf("GiveFromISR\n");
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t given = SEM_GIVE_ISR(&xHigherPriorityTaskWoken);

    if (GiveFromISRTestCalls == 0) {
        configASSERT(given == pdTRUE);
        configASSERT(xHigherPriorityTaskWoken == pdFALSE);
    } else if (GiveFromISRTestCalls == 1) {
        configASSERT(given == pdFALSE);
        configASSERT(xHigherPriorityTaskWoken == pdFALSE);
    } else if (GiveFromISRTestCalls == 2) {
        configASSERT(given == pdTRUE);
        configASSERT(xHigherPriorityTaskWoken == pdTRUE);
    }

    ++GiveFromISRTestCalls;
}

// indicates if low priority task should call GiveFromISR
int CallInteruptGiveFromISR = 0;

void GiveFromISRHighPriorityTaskFunc(void* pvParamaters) {
    (void) pvParamaters;

    // semaphore has initial count of 1 and max count of 2
    // summon interrupt to call GiveFromISR twice
    // first GiveFromISR should succeed, second should fail
    CallIRQN(GIVE_FROM_ISR_IRQN, 1);
    CallIRQN(GIVE_FROM_ISR_IRQN, 1);

    // count of semaphore should be 2 so 2 takes should be successful
    printf("TAKE\n");
    configASSERT(SEM_TAKE() == pdTRUE);
    printf("TAKE\n");
    configASSERT(SEM_TAKE() == pdTRUE);

    // semaphore is now empty so we must wait now for GiveFromISR to take the
    // sempahore. when we wait to take, lower priority will go and we should
    // indicate to it to call the GiveFromISR interrupt
    CallInteruptGiveFromISR = 1;
    printf("Waiting to TAKE\n");
    configASSERT(SEM_TAKE() == pdTRUE);
    printf("Done TAKE\n");

    vTaskDelete(NULL);
}

void GiveFromISRLowPriorityTaskFunc(void* pvParamaters) {
    (void) pvParamaters;

    for (;;) {
        if (CallInteruptGiveFromISR) {
            CallIRQN(GIVE_FROM_ISR_IRQN, 1);
            vTaskDelete(NULL);
        }
    }
}

void TestGiveFromISR() {
    // Test the following
    // 1) When count < max_count, success
    // 2) When count == max_count, failure
    // 3) xHigherPriorityTaskWoken should be set appropriately

    // create counting semaphore
    UBaseType_t max_count = 2;
    UBaseType_t init_count = 1;

    #if (USE_MY_SEM == 1)
        MySemaphore = pxMySemaphoreCreate(max_count, init_count);
        configASSERT(MySemaphore);
    #else
        xSemaphore = xSemaphoreCreateCounting(max_count, init_count);
        configASSERT(xSemaphore);
    #endif

    xTaskCreate(GiveFromISRHighPriorityTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
    xTaskCreate(GiveFromISRLowPriorityTaskFunc,
                NULL,
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    vTaskStartScheduler();
}
