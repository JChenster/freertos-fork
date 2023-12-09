#include "FreeRTOS.h"
#include <stdio.h>

#include "my_semaphore.h"
#include "task.h"

struct MySemaphoreDefinition
{
    UBaseType_t uxCount;
    UBaseType_t uxMaxCount;

    /* List of givers (takers) ordered by priority and then when task calls
     * give (take) */
    List_t xWaitingGivers;
    List_t xWaitingTakers;
};
/*-----------------------------------------------------------*/

MySemaphoreHandle_t pxMySemaphoreCreate( const UBaseType_t uxMaxCount,
                                         const UBaseType_t uxInitialCount )
{
    MySemaphoreHandle_t pxNewSemaphore = pvPortMalloc( sizeof( MySemaphore_t ) );

    if( pxNewSemaphore == NULL)
    {
        return NULL;
    }

    pxNewSemaphore->uxCount = uxInitialCount;
    pxNewSemaphore->uxMaxCount = uxMaxCount;

    vListInitialise( &( pxNewSemaphore->xWaitingGivers ) );
    vListInitialise( &( pxNewSemaphore->xWaitingTakers ) );

    return pxNewSemaphore;
}
/*-----------------------------------------------------------*/

void vMySemaphoreDelete( MySemaphoreHandle_t pxMySemaphore ) {
    /* Check semaphore is non-null */
    configASSERT( pxMySemaphore );

    vPortFree( pxMySemaphore );
}
/*-----------------------------------------------------------*/

BaseType_t xMySemaphoreTake( MySemaphoreHandle_t pxMySemaphore,
                             TickType_t xTicksToWait )
{
    /* Check semaphore is non-null */
    configASSERT( pxMySemaphore );

    /* Semaphore modification operations must be done under critical sections */
    taskENTER_CRITICAL();

    if( pxMySemaphore->uxCount > 0 )
    {
        /* Semaphore resource is available so take it */
        ( pxMySemaphore->uxCount )--;

        /* Since resource can no longer be full, if there are any waiting givers,
         * they are now able to give the resource */
        if( listLIST_IS_EMPTY( &( pxMySemaphore->xWaitingGivers ) ) == pdFALSE )
        {
            /* Notify the next giver that resource is no longer full. Next
             * giver gives semaphore, incrementing count of the semaphore */
            vTaskPopFromSemaphoreList( &( pxMySemaphore->xWaitingGivers ) );
            ( pxMySemaphore->uxCount )++;
        }

        taskEXIT_CRITICAL();
        return pdTRUE;
    }

    /* Semaphore resource is not available so add to list of waiting takers */
    vTaskPlaceOnSemaphoreList( &( pxMySemaphore->xWaitingTakers ) );

    /* Exit critical section to allow task to be notified */
    taskEXIT_CRITICAL();

    /* Wait to be notified that semaphore is available with timeout */
    uint32_t ulNotifiedValue = ulTaskNotifyTake( pdTRUE, xTicksToWait );

    /* Task is now unblocked. If semaphore was taken, ulNotifiedValue will be
     * non-zero and give function will have already decremented semaphore count */
    if( ulNotifiedValue > 0)
    {
        return pdTRUE;
    }

    /* If semaphore was not taken, remove task from waiting takers */
    vTaskRemoveFromSemaphoreList( &( pxMySemaphore->xWaitingTakers ) );
    return pdFALSE;
}
/*-----------------------------------------------------------*/

BaseType_t xMySemaphoreGive( MySemaphoreHandle_t pxMySemaphore,
                             TickType_t xTicksToWait )
{
    /* Check semaphore is non-null */
    configASSERT( pxMySemaphore );

    /* Semaphore modification operations must be done under critical sections */
    taskENTER_CRITICAL();

    if( pxMySemaphore->uxCount < pxMySemaphore->uxMaxCount )
    {
        /* Semaphore resource is not full so give */
        ( pxMySemaphore->uxCount )++;

        /* Since resource can no longer be empty, if there are any waiting
         * takers, they are now able to take the resource */
        if( listLIST_IS_EMPTY( &( pxMySemaphore->xWaitingTakers ) ) == pdFALSE )
        {
            /* Notify the next taker that semaphore is ready. Next taker takes
             * semaphore, decrementing count of the semaphore */
            vTaskPopFromSemaphoreList( &( pxMySemaphore->xWaitingTakers ) );
            ( pxMySemaphore->uxCount )--;
        }

        taskEXIT_CRITICAL();
        return pdTRUE;
    }

    /* Semaphore resource full so add to list of waiting givers */
    vTaskPlaceOnSemaphoreList( &( pxMySemaphore->xWaitingGivers ) );

    /* Exit critical section to allow task to be notified */
    taskEXIT_CRITICAL();

    /* Wait to be notified that resource is not full with timeout */
    uint32_t ulNotifiedValue = ulTaskNotifyTake( pdTRUE, xTicksToWait );

    /* Task is now unblocked. If semaphore was given, ulNotifiedValue will be
     * non-zero and take function will have already incremented semaphore count */
    if( ulNotifiedValue > 0)
    {
        return pdTRUE;
    }

    /* If semaphore was not given, remove task from waiting givers */
    vTaskRemoveFromSemaphoreList( &( pxMySemaphore->xWaitingGivers ) );
    return pdFALSE;
}
/*-----------------------------------------------------------*/

BaseType_t xMySemaphoreTakeFromISR( MySemaphoreHandle_t pxMySemaphore,
                                    BaseType_t* pxHigherPriorityTaskWoken )
{
    /* Taking semaphore from ISR works exactly the same as without ISR except
     * we do not wait for semaphore if it is empty */
    configASSERT( pxMySemaphore );

    BaseType_t taken = pdFALSE;

    /* ISR must use special critical section */
    UBaseType_t xSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

    if( pxMySemaphore->uxCount > 0 )
    {
        ( pxMySemaphore->uxCount )--;

        if( listLIST_IS_EMPTY( &( pxMySemaphore->xWaitingGivers ) ) == pdFALSE )
        {
            BaseType_t xWoken =
                xTaskPopFromSemaphoreListFromISR( &( pxMySemaphore->xWaitingGivers ) );
            ( pxMySemaphore->uxCount )++;

            /* Set pxHigherPriorityTaskWoken if not NULL */
            if ( pxHigherPriorityTaskWoken != NULL )
            {
                *pxHigherPriorityTaskWoken = xWoken;
            }
        }

        taken = pdTRUE;
    }

    taskEXIT_CRITICAL_FROM_ISR( xSavedInterruptStatus );
    return taken;
}
/*-----------------------------------------------------------*/

BaseType_t xMySemaphoreGiveFromISR( MySemaphoreHandle_t pxMySemaphore,
                                    BaseType_t* pxHigherPriorityTaskWoken )
{
    /* Taking semaphore from ISR works exactly the same as without ISR except
     * we do not wait for semaphore if it is full */
    configASSERT( pxMySemaphore );

    BaseType_t given = pdFALSE;

    /* ISR must use special critical section */
    UBaseType_t xSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

    if( pxMySemaphore->uxCount < pxMySemaphore->uxMaxCount )
    {
        ( pxMySemaphore->uxCount )++;

        if ( listLIST_IS_EMPTY( &( pxMySemaphore->xWaitingTakers ) ) == pdFALSE )
        {
            BaseType_t xWoken =
                xTaskPopFromSemaphoreListFromISR( &( pxMySemaphore->xWaitingTakers ) );

            // Set HigherPriorityTaskWoken indicator here if not NULL
            if ( pxHigherPriorityTaskWoken != NULL )
            {
                *pxHigherPriorityTaskWoken = xWoken;
            }

            ( pxMySemaphore->uxCount )--;
        }

        given = pdTRUE;
    }

    taskEXIT_CRITICAL_FROM_ISR( xSavedInterruptStatus );
    return given;
}
/*-----------------------------------------------------------*/

BaseType_t xMySemaphoreTakeAvailableFromISR( MySemaphoreHandle_t pxMySemaphore )
{
    configASSERT( pxMySemaphore );

    UBaseType_t xSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    BaseType_t xAvailable = ( pxMySemaphore->uxCount > 0 ) ? pdTRUE : pdFALSE;
    taskEXIT_CRITICAL_FROM_ISR( xSavedInterruptStatus );
    return xAvailable;
}
/*-----------------------------------------------------------*/

BaseType_t xMySemaphoreGiveAvailableFromISR( MySemaphoreHandle_t pxMySemaphore )
{
    configASSERT( pxMySemaphore );

    UBaseType_t xSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    BaseType_t xAvailable =
        ( pxMySemaphore->uxCount < pxMySemaphore->uxMaxCount ) ? pdTRUE : pdFALSE;
    taskEXIT_CRITICAL_FROM_ISR( xSavedInterruptStatus );
    return xAvailable;
}
