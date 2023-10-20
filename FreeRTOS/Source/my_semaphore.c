#include "FreeRTOS.h"
#include <stdio.h>

#include "my_semaphore.h"
#include "task.h"

// TODO: is this necessary?
/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
 * all the API functions to use the MPU wrappers.  That should only be done when
 * task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

struct MySemaphoreDefinition {
    UBaseType_t Count;
    UBaseType_t MaxCount;

    // list of givers (takers) ordered by when task calls give (take)
    List_t WaitingGivers;
    List_t WaitingTakers;
};

MySemaphoreHandle_t MySemaphoreCreate(const UBaseType_t MaxCount,
                                      const UBaseType_t InitialCount)
{
    MySemaphoreHandle_t NewSemaphore = pvPortMalloc(sizeof(MySemaphore_t));

    if (NewSemaphore == NULL) {
        return NULL;
    }

    NewSemaphore->Count = InitialCount;
    NewSemaphore->MaxCount = MaxCount;

    vListInitialise(&(NewSemaphore->WaitingGivers));
    vListInitialise(&(NewSemaphore->WaitingTakers));

    return NewSemaphore;
}

BaseType_t MySemaphoreTake(MySemaphoreHandle_t MySemaphore) {
    // Check semaphore is non-null
    configASSERT(MySemaphore);

    taskENTER_CRITICAL();

    if (MySemaphore->Count > 0) {
        // Semaphore resource is available so take it
        --(MySemaphore->Count);

        // Since resource can no longer be full, if there are any waiting
        // givers, they are now able to give the resource
        if (listLIST_IS_EMPTY(&(MySemaphore->WaitingGivers)) == pdFALSE) {
            // TODO
        }

        taskEXIT_CRITICAL();
        return pdTRUE;
    }

    // Semaphore resource is not available so add to list of waiting takers
    vTaskPlaceOnSemList(&(MySemaphore->WaitingTakers), pdTRUE);

    // exit critical section to allow task to be notified
    taskEXIT_CRITICAL();

    // wait to be notified that semaphore is available
    const TickType_t xBlockTime = pdMS_TO_TICKS(10000);
    uint32_t ulNotifiedValue = ulTaskNotifyTake(pdTRUE, xBlockTime);

    // task is now unblocked
    // if semaphore was obtained, ulNotifiedValue will be non-zero and give
    // function will have already decremented semaphore count
    return ulNotifiedValue > 0;
}

BaseType_t MySemaphoreGive(MySemaphoreHandle_t MySemaphore) {
    // Check semaphore is non-null
    configASSERT(MySemaphore);

    BaseType_t given = pdFALSE;
    taskENTER_CRITICAL();

    if (MySemaphore->Count < MySemaphore->MaxCount) {
        // Resource is not full so give
        ++(MySemaphore->Count);
        given = pdTRUE;

        // Since resource can no longer be empty, if there are any waiting
        // takers, they are now able to take the resource
        if (listLIST_IS_EMPTY(&(MySemaphore->WaitingTakers)) == pdFALSE) {
            // Notify the next taker that semaphore is ready
            vTaskRemoveFromSemList(&(MySemaphore->WaitingTakers), pdTRUE);

            // Decrement count of the semaphore
            --(MySemaphore->Count);
        }
    } else {
        // Resource full so add to list of waiting givers
        // TODO
    }

    taskEXIT_CRITICAL();
    return given;
}
