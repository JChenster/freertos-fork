/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "my_semaphore.h"

/* Standard includes. */
#include <stdio.h>

#define BLOCK_MS pdMS_TO_TICKS(1000UL)
#define STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

// Identifiers for test
#define BINARY_SAME_PRIORITY (0)
#define BINARY_DIFF_PRIORITY (1)

// Set this to what test you want to run
#define RUNNING_TEST (BINARY_SAME_PRIORITY)

MySemaphoreHandle_t MySemaphore;
int Priorities[5];

// Test function declarations
void TestBinarySamePriority();
void TestBinaryDiffPriority();

void main_my_sem(void) {
    #if RUNNING_TEST == BINARY_SAME_PRIORITY
        printf("Running binary semaphore test with same priority tasks\n");
        TestBinarySamePriority();
    #elif RUNNING_TEST == BINARY_DIFF_PRIORITY
        printf("Running binary semaphore test with different priority tasks\n");
        TestBinaryDiffPriority();
    #else
        printf("Invalid RUNNING_TEST\n");
    #endif
}

static void BinaryTaskFunc(void* pvParamaters) {
    int task_num = (int) pvParamaters;

    TickType_t xNextWakeTime = xTaskGetTickCount();

    // sleep for a little so that smaller number tasks go first
    vTaskDelayUntil(&xNextWakeTime, pdMS_TO_TICKS(100UL * task_num));

    for (;;) {
        // take semaphore
        configASSERT(MySemaphoreTake(MySemaphore) == pdTRUE);
        printf("Task %d TAKE SUCCESS\n", task_num);

        // sleep and hold the semaphore
        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);

        // give the semaphore
        printf("Task %d GIVE\n", task_num);
        configASSERT(MySemaphoreGive(MySemaphore) == pdTRUE);

        // sleep before trying to take again
        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);
    }
}

// helper function for testing binary semaphore
void TestBinary(int num_tasks) {
    // create binary semaphore
    UBaseType_t max_count = 1;
    UBaseType_t initial_count = 1;
    MySemaphore = MySemaphoreCreate(max_count, initial_count);
    configASSERT(MySemaphore);

    for (int i = 0; i < num_tasks; ++i) {
        xTaskCreate(BinaryTaskFunc,
                    NULL,
                    STACK_SIZE,
                    (void*) i + 1,
                    Priorities[i], // assume that Priorities is configured
                    NULL);
    }

    vTaskStartScheduler();
}

void TestBinarySamePriority() {
    Priorities[0] = tskIDLE_PRIORITY + 1;
    Priorities[1] = tskIDLE_PRIORITY + 1;

    TestBinary(2);
}

void TestBinaryDiffPriority() {
    Priorities[0] = tskIDLE_PRIORITY + 1;
    Priorities[1] = tskIDLE_PRIORITY + 2;

    TestBinary(2);
}
