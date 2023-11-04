#include "FreeRTOS.h"
#include <stdio.h>

#include "my_semaphore.h"
#include "task.h"

struct MySemaphoreDefinition {
    UBaseType_t Count;
    UBaseType_t MaxCount;

    // list of givers (takers) ordered by priority and then when task calls
    // give (take)
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

BaseType_t MySemaphoreTake(MySemaphoreHandle_t MySemaphore,
                           TickType_t TicksToWait)
{
    // Check semaphore is non-null
    configASSERT(MySemaphore);

    // enter critical section so only one task can obtain semaphore at a time
    taskENTER_CRITICAL();

    // Semaphore resource is available so take it
    if (MySemaphore->Count > 0) {
        --(MySemaphore->Count);

        // Since resource can no longer be full, if there are any waiting
        // givers, they are now able to give the resource
        if (listLIST_IS_EMPTY(&(MySemaphore->WaitingGivers)) == pdFALSE) {
            // Notify the next giver that resource is no longer full
            vTaskRemoveFromSemList(&(MySemaphore->WaitingGivers), pdFALSE);

            // Next giver gives semaphore, incrementing count of the semaphore
            ++(MySemaphore->Count);
        }

        taskEXIT_CRITICAL();
        return pdTRUE;
    }

    // Semaphore resource is not available so add to list of waiting takers
    vTaskPlaceOnSemList(&(MySemaphore->WaitingTakers), pdTRUE);

    // exit critical section to allow task to be notified
    taskEXIT_CRITICAL();

    // wait to be notified that semaphore is available with timeout of
    // TicksToWait
    uint32_t ulNotifiedValue = ulTaskNotifyTake(pdTRUE, TicksToWait);

    // task is now unblocked
    // if semaphore was obtained, ulNotifiedValue will be non-zero and give
    // function will have already decremented semaphore count
    return ulNotifiedValue > 0 ? pdTRUE : pdFALSE;
}

BaseType_t MySemaphoreGive(MySemaphoreHandle_t MySemaphore) {
    // Check semaphore is non-null
    configASSERT(MySemaphore);

    // enter critical section so only one task can give semaphore at a time
    taskENTER_CRITICAL();

    // Resource is not full so give
    if (MySemaphore->Count < MySemaphore->MaxCount) {
        ++(MySemaphore->Count);

        // Since resource can no longer be empty, if there are any waiting
        // takers, they are now able to take the resource
        if (listLIST_IS_EMPTY(&(MySemaphore->WaitingTakers)) == pdFALSE) {
            // Notify the next taker that semaphore is ready
            vTaskRemoveFromSemList(&(MySemaphore->WaitingTakers), pdTRUE);

            // Next taker takes semaphore, decrementing count of the semaphore
            --(MySemaphore->Count);
        }

        taskEXIT_CRITICAL();
        return pdTRUE;
    }

    // Semaphore resource full so add to list of waiting givers
    vTaskPlaceOnSemList(&(MySemaphore->WaitingGivers), pdFALSE);

    // exit critical section to allow task to be notified
    taskEXIT_CRITICAL();

    // wait indefinitely to be notified that resource is not full
    uint32_t ulNotifiedValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // task is now unblocked
    // if resource was given, ulNotifiedValue will be non-zero and take
    // function will have already incremented semaphore count
    return ulNotifiedValue > 0 ? pdTRUE : pdFALSE;
}
