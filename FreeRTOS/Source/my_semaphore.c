#include "FreeRTOS.h"
#include <stdio.h>

#include "my_semaphore.h"
#include "task.h"

struct MySemaphoreDefinition {
    UBaseType_t uxCount;
    UBaseType_t uxMaxCount;

    /* List of givers (takers) ordered by priority and then when task calls
     * give (take) */
    List_t xWaitingGivers;
    List_t xWaitingTakers;
};

MySemaphoreHandle_t pxMySemaphoreCreate( const UBaseType_t uxMaxCount,
                                         const UBaseType_t uxInitialCount )
{
    MySemaphoreHandle_t pxNewSemaphore = pvPortMalloc(sizeof(MySemaphore_t));

    if (pxNewSemaphore == NULL) {
        return NULL;
    }

    pxNewSemaphore->uxCount = uxInitialCount;
    pxNewSemaphore->uxMaxCount = uxMaxCount;

    vListInitialise(&(pxNewSemaphore->xWaitingGivers));
    vListInitialise(&(pxNewSemaphore->xWaitingTakers));

    return pxNewSemaphore;
}

void vMySemaphoreDelete( MySemaphoreHandle_t pxMySemaphore ) {
    /* Check semaphore is non-null */
    configASSERT(pxMySemaphore);

    vPortFree(pxMySemaphore);
}

BaseType_t xMySemaphoreTake( MySemaphoreHandle_t pxMySemaphore,
                             TickType_t xTicksToWait )
{
    /* Check semaphore is non-null */
    configASSERT(pxMySemaphore);

    /* Semaphore modification operations must be done under critical sections */
    taskENTER_CRITICAL();

    if (pxMySemaphore->uxCount > 0) {
        /* Semaphore resource is available so take it */
        --(pxMySemaphore->uxCount);

        /* Since resource can no longer be full, if there are any waiting givers,
         * they are now able to give the resource */
        if (listLIST_IS_EMPTY(&(pxMySemaphore->xWaitingGivers)) == pdFALSE) {
            /* Notify the next giver that resource is no longer full
             * Next giver gives semaphore, incrementing count of the semaphore */
            vTaskRemoveFromSemList(&(pxMySemaphore->xWaitingGivers), pdFALSE);
            ++(pxMySemaphore->uxCount);
        }

        taskEXIT_CRITICAL();
        return pdTRUE;
    }

    /* Semaphore resource is not available so add to list of waiting takers */
    vTaskPlaceOnSemList(&(pxMySemaphore->xWaitingTakers), pdTRUE);

    /* Exit critical section to allow task to be notified */
    taskEXIT_CRITICAL();

    /* Wait to be notified that semaphore is available with timeout */
    uint32_t ulNotifiedValue = ulTaskNotifyTake(pdTRUE, xTicksToWait);

    /* Task is now unblocked. If semaphore was obtained, ulNotifiedValue will be
     * non-zero and give function will have already decremented semaphore count */
    return ulNotifiedValue > 0 ? pdTRUE : pdFALSE;
}

BaseType_t MySemaphoreGive(MySemaphoreHandle_t MySemaphore,
                           TickType_t TicksToWait)
{
    // Check semaphore is non-null
    configASSERT(MySemaphore);

    // enter critical section so only one task can give semaphore at a time
    taskENTER_CRITICAL();

    // Resource is not full so give
    if (MySemaphore->uxCount < MySemaphore->uxMaxCount) {
        ++(MySemaphore->uxCount);

        // Since resource can no longer be empty, if there are any waiting
        // takers, they are now able to take the resource
        if (listLIST_IS_EMPTY(&(MySemaphore->xWaitingTakers)) == pdFALSE) {
            // Notify the next taker that semaphore is ready
            vTaskRemoveFromSemList(&(MySemaphore->xWaitingTakers), pdTRUE);

            // Next taker takes semaphore, decrementing count of the semaphore
            --(MySemaphore->uxCount);
        }

        taskEXIT_CRITICAL();
        return pdTRUE;
    }

    // Semaphore resource full so add to list of waiting givers
    vTaskPlaceOnSemList(&(MySemaphore->xWaitingGivers), pdFALSE);

    // exit critical section to allow task to be notified
    taskEXIT_CRITICAL();

    // wait indefinitely to be notified that resource is not full
    uint32_t ulNotifiedValue = ulTaskNotifyTake(pdTRUE, TicksToWait);

    // task is now unblocked
    // if resource was given, ulNotifiedValue will be non-zero and take
    // function will have already incremented semaphore count
    return ulNotifiedValue > 0 ? pdTRUE : pdFALSE;
}

BaseType_t MySemaphoreTakeFromISR(MySemaphoreHandle_t MySemaphore,
                                  BaseType_t* HigherPriorityTaskWoken)
{
    // Taking semaphore from ISR works exactly the same as without ISR except
    // we do not wait for semaphore if it is empty
    configASSERT(MySemaphore);

    BaseType_t taken = pdFALSE;

    // ISR must use special critical section
    UBaseType_t SavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

    if (MySemaphore->uxCount > 0) {
        --(MySemaphore->uxCount);

        if (listLIST_IS_EMPTY(&(MySemaphore->xWaitingGivers)) == pdFALSE) {
            BaseType_t IsHigherPriorityTaskWoken =
                vTaskRemoveFromSemListFromISR(&(MySemaphore->xWaitingGivers), pdFALSE);

            // Set HigherPriorityTaskWoken indicator here if not NULL
            if (HigherPriorityTaskWoken != NULL) {
                *HigherPriorityTaskWoken = IsHigherPriorityTaskWoken;
            }

            ++(MySemaphore->uxCount);
        }

        taken = pdTRUE;
    }

    taskEXIT_CRITICAL_FROM_ISR(SavedInterruptStatus);
    return taken;
}

BaseType_t MySemaphoreGiveFromISR(MySemaphoreHandle_t MySemaphore,
                                  BaseType_t* HigherPriorityTaskWoken)
{
    // Taking semaphore from ISR works exactly the same as without ISR except
    // we do not wait for semaphore if it is full
    configASSERT(MySemaphore);

    BaseType_t given = pdFALSE;

    // ISR must use special critical section
    UBaseType_t SavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

    if (MySemaphore->uxCount < MySemaphore->uxMaxCount) {
        ++(MySemaphore->uxCount);

        if (listLIST_IS_EMPTY(&(MySemaphore->xWaitingTakers)) == pdFALSE) {
            BaseType_t IsHigherPriorityTaskWoken =
                vTaskRemoveFromSemListFromISR(&(MySemaphore->xWaitingTakers), pdTRUE);

            // Set HigherPriorityTaskWoken indicator here if not NULL
            if (HigherPriorityTaskWoken != NULL) {
                *HigherPriorityTaskWoken = IsHigherPriorityTaskWoken;
            }

            --(MySemaphore->uxCount);
        }

        given = pdTRUE;
    }

    taskEXIT_CRITICAL_FROM_ISR(SavedInterruptStatus);
    return given;
}

BaseType_t MySemaphoreTakeAvailableFromISR(MySemaphoreHandle_t MySemaphore) {
    configASSERT(MySemaphore);

    UBaseType_t SavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    BaseType_t Available = MySemaphore->uxCount > 0 ? pdTRUE : pdFALSE;
    taskEXIT_CRITICAL_FROM_ISR(SavedInterruptStatus);
    return Available;
}

BaseType_t MySemaphoreGiveAvailableFromISR(MySemaphoreHandle_t MySemaphore) {
    configASSERT(MySemaphore);

    UBaseType_t SavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    BaseType_t Available = MySemaphore->uxCount < MySemaphore->uxMaxCount ? pdTRUE
                                                                      : pdFALSE;
    taskEXIT_CRITICAL_FROM_ISR(SavedInterruptStatus);
    return Available;
}
