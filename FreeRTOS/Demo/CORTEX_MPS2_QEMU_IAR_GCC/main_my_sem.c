/* Standard includes. */
#include <assert.h>
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "my_semaphore.h"

#define BLOCK_MS pdMS_TO_TICKS(1000UL)

static void proc1(void* pvParamaters);
static void proc2(void* pvParamaters);

MySemaphoreHandle_t MySemaphore;

void main_my_sem(void) {
    MySemaphore = MySemaphoreCreate(2, 1);

    if (MySemaphore) {
        printf("MySemaphore successfully set up\n");

        xTaskCreate(proc1,
                    "proc1",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    tskIDLE_PRIORITY + 1,
                    NULL);

        xTaskCreate(proc2,
                    "proc2",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    tskIDLE_PRIORITY + 1,
                    NULL);
    }

    vTaskStartScheduler();
}

static void proc1(void* pvParamaters) {
    (void) pvParamaters;

    TickType_t xNextWakeTime = xTaskGetTickCount();

    for (;;) {
        BaseType_t taken = MySemaphoreTake(MySemaphore);

        if (taken) {
            printf("Process 1 just took semaphore\n");
        } else {
            printf("Process 1 timed out waiting for take\n");
        }

        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);

        if (taken == pdTRUE) {
            printf("Process 1 gonna give it up now\n");
            assert(MySemaphoreGive(MySemaphore) == pdTRUE);
        }

        // wait before trying to take again
        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);
    }
}

static void proc2(void* pvParamaters) {
    (void) pvParamaters;

    TickType_t xNextWakeTime = xTaskGetTickCount();

    for (;;) {
        BaseType_t taken = MySemaphoreTake(MySemaphore);

        if (taken) {
            printf("Process 2 just took semaphore\n");
        } else {
            printf("Process 2 timed out waiting for take\n");
        }

        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);

        if (taken == pdTRUE) {
            printf("Process 2 gonna give it up now\n");
            assert(MySemaphoreGive(MySemaphore) == pdTRUE);
        }

        // wait before trying to take again
        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);
    }
}

