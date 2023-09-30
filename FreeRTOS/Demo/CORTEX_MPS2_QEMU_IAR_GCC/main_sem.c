/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#define BLOCK_MS pdMS_TO_TICKS(1000UL)
#define SEM_WAIT_MS pdMS_TO_TICKS(3000UL)

SemaphoreHandle_t xSemaphore = NULL;

static void proc1(void* pvParamaters);
static void proc2(void* pvParamaters);

void main_sem ( void )
{
    vSemaphoreCreateBinary( xSemaphore );

    if( xSemaphore != NULL )
    {
        printf( "sem successfully set up\n" );

        xTaskCreate(proc1,
                    "proc1",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    tskIDLE_PRIORITY + 1,
                    NULL);

        xTaskCreate(proc2,
                    "proc2",
                    configMINIMAL_STACK_SIZE,
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
        xSemaphoreTake(xSemaphore, SEM_WAIT_MS);
        printf("Process 1 just took semaphore\n");

        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);

        printf("Process 1 gonna give it up now\n");
        xSemaphoreGive(xSemaphore);

        // wait before trying to take again
        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);
    }
}

static void proc2(void* pvParamaters) {
    (void) pvParamaters;

    TickType_t xNextWakeTime = xTaskGetTickCount();

    for (;;) {
        xSemaphoreTake(xSemaphore, SEM_WAIT_MS);
        printf("Process 2 just took semaphore\n");

        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);

        printf("Process 2 gonna give it up now\n");
        xSemaphoreGive(xSemaphore);

        // wait before trying to take again
        vTaskDelayUntil(&xNextWakeTime, BLOCK_MS);
    }
}

