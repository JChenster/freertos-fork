#include "FreeRTOS.h"

#include "my_queue.h"
#include "my_semaphore.h"

#include <string.h>

struct MyQueueDefinition
{
    /* Counting semaphore representing how many spots in the queue are empty */
    MySemaphoreHandle_t pxEmptySemaphore;
    /* Counting semaphore representing how many spots in the queue are full */
    MySemaphoreHandle_t pxFullSemaphore;
    /* Binary semaphore that provides mutual exclusivity while reading / writing */
    MySemaphoreHandle_t pxModifySemaphore;

    size_t xItemSize;

    /* Circular buffer. We read from ucHead and write to ucTail */
    int8_t* ucHead;
    int8_t* ucTail;

    /* Optimization */
    int8_t* ucBufferBegin;
    int8_t* ucBufferEnd;
};
/*-----------------------------------------------------------*/

MyQueueHandle_t pxMyQueueCreate( UBaseType_t xQueueLength, UBaseType_t xItemSize )
{
    MyQueueHandle_t pxNewQueue = NULL;

    if( xQueueLength > 0 &&
        /* Check for overflow */
        ( SIZE_MAX / xQueueLength ) >= xItemSize &&
        ( ( size_t ) ( SIZE_MAX - sizeof( MyQueue_t ) ) ) >= ( ( size_t ) (xQueueLength * xItemSize) ) )
    {
        size_t xQueueSizeBytes = sizeof( MyQueue_t ) + xQueueLength + xItemSize;
        pxNewQueue = pvPortMalloc( xQueueSizeBytes );

        if( pxNewQueue == NULL )
        {
            return NULL;
        }

        /* Initialize semaphores */
        pxNewQueue->pxEmptySemaphore = pxMySemaphoreCreate( xQueueLength, xQueueLength );
        pxNewQueue->pxFullSemaphore = pxMySemaphoreCreate( xQueueLength, 0 );
        pxNewQueue->pxModifySemaphore = pxMySemaphoreCreate( 1, 1 );

        /* If any initialization of semaphores fail, gracefully free and return */
        if( pxNewQueue->pxEmptySemaphore == NULL ||
            pxNewQueue->pxFullSemaphore == NULL ||
            pxNewQueue->pxModifySemaphore == NULL )
        {
            if( pxNewQueue->pxEmptySemaphore != NULL )
            {
                vMySemaphoreDelete( pxNewQueue->pxFullSemaphore );
            }

            if( pxNewQueue->pxEmptySemaphore != NULL )
            {
                vMySemaphoreDelete( pxNewQueue->pxEmptySemaphore );
            }

            if( pxNewQueue->pxModifySemaphore != NULL )
            {
                vMySemaphoreDelete(pxNewQueue->pxEmptySemaphore);
            }

            return NULL;
        }

        pxNewQueue->xItemSize = xItemSize;

        pxNewQueue->ucBufferBegin = ( ( int8_t* ) pxNewQueue ) + sizeof( MyQueue_t );
        pxNewQueue->ucBufferEnd = pxNewQueue->ucBufferBegin + xQueueLength * xItemSize;

        pxNewQueue->ucHead = pxNewQueue->ucBufferBegin;
        pxNewQueue->ucTail = pxNewQueue->ucBufferBegin;
    }

    return pxNewQueue;
}
/*-----------------------------------------------------------*/

void vMyQueueDelete( MyQueueHandle_t pxMyQueue ) {
    configASSERT( pxMyQueue );

    configASSERT( pxMyQueue->pxEmptySemaphore );
    configASSERT( pxMyQueue->pxFullSemaphore );
    configASSERT( pxMyQueue->pxModifySemaphore );

    vMySemaphoreDelete( pxMyQueue->pxEmptySemaphore );
    vMySemaphoreDelete( pxMyQueue->pxFullSemaphore );
    vMySemaphoreDelete( pxMyQueue->pxModifySemaphore );
}
/*-----------------------------------------------------------*/

BaseType_t xMyQueueSendToBack( MyQueueHandle_t pxMyQueue,
                               const void* pvItemToQueue,
                               TickType_t xTicksToWait )
{
    /* Get an empty slot to push an item */
    if( xMySemaphoreTake( pxMyQueue->pxEmptySemaphore, xTicksToWait / 2 ) == pdTRUE )
    {
        /* Get mutually exclusive access */
        if( xMySemaphoreTake( pxMyQueue->pxModifySemaphore, xTicksToWait / 2 ) == pdTRUE )
        {
            /* Write to ucTail and then increment ucTail */
            memcpy( ( void* ) pxMyQueue->ucTail, pvItemToQueue, pxMyQueue->xItemSize );

            pxMyQueue->ucTail += pxMyQueue->xItemSize;
            if( pxMyQueue->ucTail == pxMyQueue->ucBufferEnd )
            {
                pxMyQueue->ucTail = pxMyQueue->ucBufferBegin;
            }

            /* Done writing */
            xMySemaphoreGive( pxMyQueue->pxModifySemaphore, portMAX_DELAY );

            /* One more slot of the queue is now full */
            xMySemaphoreGive( pxMyQueue->pxFullSemaphore, portMAX_DELAY );

            return pdTRUE;
        }
        else
        {
            /* Take pxModifySemaphore failed so give back the empty slot we took */
            xMySemaphoreGive( pxMyQueue->pxEmptySemaphore, portMAX_DELAY );
        }
    }

    return errQUEUE_FULL;
}
/*-----------------------------------------------------------*/

BaseType_t xMyQueueReceive( MyQueueHandle_t pxMyQueue,
                            void* pvBuffer,
                            TickType_t xTicksToWait )
{
    /* Get a full slot to pop an item */
    if( xMySemaphoreTake( pxMyQueue->pxFullSemaphore, xTicksToWait / 2 ) == pdTRUE )
    {
        /* Get mutual exclusive access */
        if( xMySemaphoreTake( pxMyQueue->pxModifySemaphore, xTicksToWait / 2 ) == pdTRUE )
        {
            /* Read from ucHead and then increment ucHead */
            memcpy( pvBuffer, ( void* ) pxMyQueue->ucHead, pxMyQueue->xItemSize );

            pxMyQueue->ucHead += pxMyQueue->xItemSize;
            if( pxMyQueue->ucHead == pxMyQueue->ucBufferEnd )
            {
                pxMyQueue->ucHead = pxMyQueue->ucBufferBegin;
            }

            /* Done reading */
            xMySemaphoreGive( pxMyQueue->pxModifySemaphore, portMAX_DELAY );

            /* One more slot of the queue is now empty */
            xMySemaphoreGive( pxMyQueue->pxEmptySemaphore, portMAX_DELAY );

            return pdTRUE;
        }
        else
        {
            /* Take pxModifySemaphore failed so give back the full slot we took */
            xMySemaphoreGive( pxMyQueue->pxFullSemaphore, portMAX_DELAY );
        }
    }

    return errQUEUE_EMPTY;
}
/*-----------------------------------------------------------*/

BaseType_t xMyQueueSendToBackFromISR( MyQueueHandle_t pxMyQueue,
                                      const void* pvItemToQueue,
                                      BaseType_t* pxHigherPriorityTaskWoken )
{
    BaseType_t xStatus = errQUEUE_FULL;

    /* Attempt to take pxModifySemaphore without waiting. Taking
     * pxModifySemaphore should not awaken any givers since the giver of
     * pxModifySemaphore is always the taker so it always succeeds and never
     * waits so pxHigherPriorityTaskWoken parameter is irrelevant here */
    if( xMySemaphoreTakeFromISR( pxMyQueue->pxModifySemaphore, NULL ) == pdTRUE )
    {
        /* We can assume that what happens in this block occurs atomically
         * since only other interrupts can interrupt this function but they
         * would not be able to acquire pxModifySemaphore */

        /* Take and give functions may notify other tasks so we should not call
         * take and give unless we are confident they will succeed */
        if( xMySemaphoreTakeAvailableFromISR( pxMyQueue->pxEmptySemaphore ) == pdTRUE &&
            xMySemaphoreGiveAvailableFromISR( pxMyQueue->pxFullSemaphore ) == pdTRUE )
        {
            BaseType_t EmptyWoken = pdFALSE;
            BaseType_t FullWoken = pdFALSE;

            /* Take pxEmptySemaphore to push an item and ensure it succeeds */
            configASSERT( xMySemaphoreTakeFromISR( pxMyQueue->pxEmptySemaphore, &EmptyWoken ) == pdTRUE );

            /* Write to ucTail and then increment ucTail */
            memcpy( ( void* ) pxMyQueue->ucTail, pvItemToQueue, pxMyQueue->xItemSize );

            pxMyQueue->ucTail += pxMyQueue->xItemSize;
            if( pxMyQueue->ucTail == pxMyQueue->ucBufferEnd )
            {
                pxMyQueue->ucTail = pxMyQueue->ucBufferBegin;
            }

            /* Give pxEmptySemaphore to signal that an item has been pushed and
             * ensure it succeeds */
            configASSERT( xMySemaphoreGiveFromISR( pxMyQueue->pxFullSemaphore, &FullWoken ) == pdTRUE );

            /* Set pxHigherPriorityTaskWoken indicator if not NULL */
            if( pxHigherPriorityTaskWoken != NULL )
            {
                *pxHigherPriorityTaskWoken =
                    ( ( EmptyWoken == pdTRUE ) || ( FullWoken == pdTRUE ) ) ? pdTRUE : pdFALSE;
            }

            /* Send was successful */
            xStatus = pdTRUE;
        }

        /* Done modifying. Should always succeed */
        configASSERT( xMySemaphoreGiveFromISR( pxMyQueue->pxModifySemaphore, NULL ) == pdTRUE );
    }

    return xStatus;
}
/*-----------------------------------------------------------*/

BaseType_t xMyQueueReceiveFromISR( MyQueueHandle_t pxMyQueue,
                                   void* pvBuffer,
                                   BaseType_t* pxHigherPriorityTaskWoken )
{
    BaseType_t xStatus = errQUEUE_EMPTY;

    /* Attempt to take pxModifySemaphore without waiting. Taking
     * pxModifySemaphore should not awaken any givers since the giver of
     * pxModifySemaphore is always the taker so it always succeeds and never
     * waits so pxHigherPriorityTaskWoken parameter is irrelevant here */
    if( xMySemaphoreTakeFromISR( pxMyQueue->pxModifySemaphore, NULL ) == pdTRUE )
    {
        /* We can assume that what happens in this block occurs atomically
         * since only other interrupts can interrupt this function but they
         * would not be able to acquire pxModifySemaphore */

        /* Take and give functions may notify other tasks so we should not call
         * take and give unless we are confident they will succeed */
        if( xMySemaphoreTakeAvailableFromISR( pxMyQueue->pxFullSemaphore ) == pdTRUE &&
            xMySemaphoreGiveAvailableFromISR( pxMyQueue->pxEmptySemaphore ) == pdTRUE )
        {
            BaseType_t FullWoken = pdFALSE;
            BaseType_t EmptyWoken = pdFALSE;

            /* Take pxFullSemaphore to push an item and ensure it succeeds */
            configASSERT( xMySemaphoreTakeFromISR( pxMyQueue->pxFullSemaphore, &FullWoken ) == pdTRUE );

            /* Read from ucHead and then increment ucHead */
            memcpy( pvBuffer, ( void* ) pxMyQueue->ucHead, pxMyQueue->xItemSize );

            pxMyQueue->ucHead += pxMyQueue->xItemSize;
            if( pxMyQueue->ucHead == pxMyQueue->ucBufferEnd )
            {
                pxMyQueue->ucHead = pxMyQueue->ucBufferBegin;
            }

            /* Give pxEmptySemaphore to signal that an item has been pushed and
             * ensure it succeeds */
            configASSERT( xMySemaphoreGiveFromISR( pxMyQueue->pxEmptySemaphore, &EmptyWoken ) == pdTRUE );

            /* Set pxHigherPriorityTaskWoken indicator if not NULL */
            if( pxHigherPriorityTaskWoken != NULL )
            {
                *pxHigherPriorityTaskWoken =
                    ( ( FullWoken == pdTRUE ) || ( EmptyWoken == pdTRUE ) ) ? pdTRUE : pdFALSE;
            }

            /* Receive was successful */
            xStatus = pdTRUE;
        }

        /* Done modifying. Should always succeed */
        xMySemaphoreGiveFromISR( pxMyQueue->pxModifySemaphore, NULL );
    }

    return xStatus;
}
