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

MyQueueHandle_t pxMyQueueCreate( UBaseType_t xQueueLength, UBaseType_t xItemSize )
{
    MyQueueHandle_t pxNewQueue = NULL;

    if( xQueueLength > 0 &&
        /* Check for overflow */
        ( SIZE_MAX / xQueueLength ) >= xItemSize &&
        ( ( size_t ) ( SIZE_MAX - sizeof( MyQueue_t ) ) ) >= ( ( size_t ) (xQueueLength * xItemSize) ) )
    {
        size_t QueueSizeBytes = sizeof( MyQueue_t ) + xQueueLength + xItemSize;
        pxNewQueue = pvPortMalloc( QueueSizeBytes );

        if( pxNewQueue == NULL )
        {
            return NULL;
        }

        pxNewQueue->pxEmptySemaphore = pxMySemaphoreCreate( xQueueLength, xQueueLength );
        pxNewQueue->pxFullSemaphore = pxMySemaphoreCreate( xQueueLength, 0 );
        pxNewQueue->pxModifySemaphore = pxMySemaphoreCreate( 1, 1 );

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

void vMyQueueDelete( MyQueueHandle_t pxMyQueue ) {
    configASSERT( pxMyQueue );

    configASSERT( pxMyQueue->pxEmptySemaphore );
    configASSERT( pxMyQueue->pxFullSemaphore );
    configASSERT( pxMyQueue->pxModifySemaphore );

    vMySemaphoreDelete( pxMyQueue->pxEmptySemaphore );
    vMySemaphoreDelete( pxMyQueue->pxFullSemaphore );
    vMySemaphoreDelete( pxMyQueue->pxModifySemaphore );
}

BaseType_t xMyQueueSendToBack( MyQueueHandle_t pxMyQueue,
                               const void* pvItemToQueue,
                               TickType_t xTicksToWait )
{
    // take pxEmptySemaphore to get an empty slot to push an item
    if( xMySemaphoreTake( pxMyQueue->pxEmptySemaphore, xTicksToWait / 2 ) == pdTRUE )
    {
        // get mutual exclusive access to pxModifySemaphore to write
        if( xMySemaphoreTake( pxMyQueue->pxModifySemaphore, xTicksToWait / 2 ) == pdTRUE )
        {
            // write to ucTail and then increment ucTail
            memcpy( ( void* ) pxMyQueue->ucTail, pvItemToQueue, pxMyQueue->xItemSize );

            pxMyQueue->ucTail += pxMyQueue->xItemSize;
            if( pxMyQueue->ucTail == pxMyQueue->ucBufferEnd )
            {
                pxMyQueue->ucTail = pxMyQueue->ucBufferBegin;
            }

            // done writing
            xMySemaphoreGive( pxMyQueue->pxModifySemaphore, portMAX_DELAY );

            // one more slot of the queue is now full
            xMySemaphoreGive( pxMyQueue->pxFullSemaphore, portMAX_DELAY );

            return pdTRUE;
        }
        else
        {
            // we were unsuccessful getting pxModifySemaphore so give back the
            // empty slot we took
            xMySemaphoreGive( pxMyQueue->pxEmptySemaphore, portMAX_DELAY );
        }
    }

    return errQUEUE_FULL;
}

BaseType_t xMyQueueReceive( MyQueueHandle_t pxMyQueue,
                            void* pvBuffer,
                            TickType_t xTicksToWait )
{
    // take pxFullSemaphore to get a full slot to pop an item
    if( xMySemaphoreTake( pxMyQueue->pxFullSemaphore, xTicksToWait / 2 ) == pdTRUE )
    {
        // get mutual exclusive access to pxModifySemaphore to read
        if( xMySemaphoreTake( pxMyQueue->pxModifySemaphore, xTicksToWait / 2 ) == pdTRUE )
        {
            // read from ucHead and then increment ucHead
            memcpy( pvBuffer, ( void* ) pxMyQueue->ucHead, pxMyQueue->xItemSize );

            pxMyQueue->ucHead += pxMyQueue->xItemSize;
            if( pxMyQueue->ucHead == pxMyQueue->ucBufferEnd )
            {
                pxMyQueue->ucHead = pxMyQueue->ucBufferBegin;
            }

            // done reading
            xMySemaphoreGive( pxMyQueue->pxModifySemaphore, portMAX_DELAY );

            // one more slot of the queue is now empty
            xMySemaphoreGive( pxMyQueue->pxEmptySemaphore, portMAX_DELAY );

            return pdTRUE;
        }
        else
        {
            // we were unsuccessful getting pxModifySemaphore so give back the
            // full slot we took
            xMySemaphoreGive( pxMyQueue->pxFullSemaphore, portMAX_DELAY );
        }
    }

    return errQUEUE_EMPTY;
}

BaseType_t xMyQueueSendToBackFromISR( MyQueueHandle_t pxMyQueue,
                                      const void* pvItemToQueue,
                                      BaseType_t* pxHigherPriorityTaskWoken )
{
    BaseType_t xStatus = errQUEUE_FULL;

    // attempt to get pxModifySemaphore without waiting
    // taking pxModifySemaphore should not awaken any givers since the giver of
    // pxModifySemaphore is always the taker so we should never have any
    // processes waiting to give so pxHigherPriorityTaskWoken parameter is
    // irrelevant here
    if( xMySemaphoreTakeFromISR( pxMyQueue->pxModifySemaphore, NULL ) == pdTRUE )
    {
        // We can assume that what happens in this block occurs atomically
        // since only other interrupts with a higher priority can interrupt
        // this function but they would not be able to acquire the ModifySmepahore

        // take and give functions may notify other tasks so we should not call
        // take and give unless we are confident they will succeed which we can
        // quickly check since we are not waiting
        if( xMySemaphoreTakeAvailableFromISR( pxMyQueue->pxEmptySemaphore ) == pdTRUE &&
            xMySemaphoreGiveAvailableFromISR( pxMyQueue->pxFullSemaphore ) == pdTRUE )
        {
            BaseType_t EmptyWoken = pdFALSE;
            BaseType_t FullWoken = pdFALSE;

            // take pxEmptySemaphore to push an item and ensure it succeeds
            configASSERT( xMySemaphoreTakeFromISR( pxMyQueue->pxEmptySemaphore, &EmptyWoken ) == pdTRUE );

            // write to ucTail and then increment ucTail
            memcpy( ( void* ) pxMyQueue->ucTail, pvItemToQueue, pxMyQueue->xItemSize );

            pxMyQueue->ucTail += pxMyQueue->xItemSize;
            if( pxMyQueue->ucTail == pxMyQueue->ucBufferEnd )
            {
                pxMyQueue->ucTail = pxMyQueue->ucBufferBegin;
            }

            // give pxFullSemaphore to indicate an item has been pushed and
            // ensure it succeeds
            configASSERT( xMySemaphoreGiveFromISR( pxMyQueue->pxFullSemaphore, &FullWoken ) == pdTRUE );

            // Set pxHigherPriorityTaskWoken indicator here if not NULL
            if( pxHigherPriorityTaskWoken != NULL )
            {
                *pxHigherPriorityTaskWoken =
                    ( ( EmptyWoken == pdTRUE ) || ( FullWoken == pdTRUE ) ) ? pdTRUE : pdFALSE;
            }

            xStatus = pdTRUE;
        }

        // done modifying, this should always succeed since we guarantee hold
        // the pxModifySemaphore
        xMySemaphoreGiveFromISR( pxMyQueue->pxModifySemaphore, NULL );
    }

    return xStatus;
}

BaseType_t xMyQueueReceiveFromISR( MyQueueHandle_t pxMyQueue,
                                   void* pvBuffer,
                                   BaseType_t* pxHigherPriorityTaskWoken )
{
    BaseType_t xStatus = errQUEUE_EMPTY;

    // attempt to get pxModifySemaphore without waiting
    // taking pxModifySemaphore should not awaken any givers since the giver of
    // pxModifySemaphore is always the taker so we should never have any
    // processes waiting to give so pxHigherPriorityTaskWoken parameter is
    // irrelevant here
    if( xMySemaphoreTakeFromISR( pxMyQueue->pxModifySemaphore, NULL ) == pdTRUE )
    {
        // We can assume that what happens in this block occurs atomically
        // since only other interrupts with a higher priority can interrupt
        // this function but they would not be able to acquire the ModifySmepahore

        // take and give functions may notify other tasks so we should not call
        // take and give unless we are confident they will succeed which we can
        // quickly check since we are not waiting
        if( xMySemaphoreTakeAvailableFromISR( pxMyQueue->pxFullSemaphore ) == pdTRUE &&
            xMySemaphoreGiveAvailableFromISR( pxMyQueue->pxEmptySemaphore ) == pdTRUE )
        {
            BaseType_t FullWoken = pdFALSE;
            BaseType_t EmptyWoken = pdFALSE;

            // take pxFullSemaphore to pop an item and ensure it succeeds
            configASSERT( xMySemaphoreTakeFromISR( pxMyQueue->pxFullSemaphore, &FullWoken ) == pdTRUE );

            // read from ucHead and then increment ucHead
            memcpy( pvBuffer, ( void* ) pxMyQueue->ucHead, pxMyQueue->xItemSize );

            pxMyQueue->ucHead += pxMyQueue->xItemSize;
            if( pxMyQueue->ucHead == pxMyQueue->ucBufferEnd )
            {
                pxMyQueue->ucHead = pxMyQueue->ucBufferBegin;
            }

            // give pxEmptySemaphore to indicate an item has been popped and
            // ensure it succeeds
            configASSERT( xMySemaphoreGiveFromISR( pxMyQueue->pxEmptySemaphore, &EmptyWoken ) == pdTRUE );

            // Set pxHigherPriorityTaskWoken indicator here if not NULL
            if( pxHigherPriorityTaskWoken != NULL )
            {
                *pxHigherPriorityTaskWoken =
                    ( ( FullWoken == pdTRUE ) || ( EmptyWoken == pdTRUE ) ) ? pdTRUE : pdFALSE;
            }

            xStatus = pdTRUE;
        }

        // done modifying, this should always succeed since we guarantee hold
        // the pxModifySemaphore
        xMySemaphoreGiveFromISR( pxMyQueue->pxModifySemaphore, NULL );
    }

    return xStatus;
}
