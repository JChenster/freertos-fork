#include "FreeRTOS.h"

#include "my_queue.h"
#include "my_semaphore.h"

#include <string.h>

struct MyQueueDefinition {
    // Synchronization
    // counting semaphore representing how many spots in the queue are empty
    MySemaphoreHandle_t EmptySemaphore;
    // counting semaphore representing how many spots in the queue are full
    MySemaphoreHandle_t FullSemaphore;
    // binary semaphore that will allow mutual exclusivity while reading / writing
    MySemaphoreHandle_t ModifySemaphore;

    size_t ItemSize;

    // circular buffer
    // we aim to read from Heead
    int8_t* Head;
    // we aim to write to Tail
    int8_t* Tail;

    // optimization
    int8_t* BufferBegin;
    int8_t* BufferEnd;
};

MyQueueHandle_t MyQueueCreate(UBaseType_t QueueLength, UBaseType_t ItemSize) {
    MyQueueHandle_t NewQueue = NULL;

    if (QueueLength > 0 &&
        // check for multiplication overflow
        (SIZE_MAX / QueueLength) >= ItemSize &&
        // check for addition overflow
        (size_t) (SIZE_MAX - sizeof(MyQueue_t)) >= (size_t) (QueueLength * ItemSize))
    {
        size_t QueueSizeBytes = sizeof(MyQueue_t) + QueueLength + ItemSize;
        NewQueue = pvPortMalloc(QueueSizeBytes);

        if (NewQueue == NULL) {
            return NULL;
        }

        NewQueue->EmptySemaphore = pxMySemaphoreCreate(QueueLength, QueueLength);
        NewQueue->FullSemaphore = pxMySemaphoreCreate(QueueLength, 0);
        NewQueue->ModifySemaphore = pxMySemaphoreCreate(1, 1);

        if (NewQueue->EmptySemaphore == NULL ||
            NewQueue->FullSemaphore == NULL ||
            NewQueue->ModifySemaphore == NULL)
        {
            if (NewQueue->EmptySemaphore != NULL) {
                vMySemaphoreDelete(NewQueue->FullSemaphore);
            }

            if (NewQueue->EmptySemaphore != NULL) {
                vMySemaphoreDelete(NewQueue->EmptySemaphore);
            }

            if (NewQueue->ModifySemaphore != NULL) {
                vMySemaphoreDelete(NewQueue->EmptySemaphore);
            }

            return NULL;
        }

        NewQueue->ItemSize = ItemSize;

        NewQueue->BufferBegin = ((int8_t*) NewQueue) + sizeof(MyQueue_t);
        NewQueue->BufferEnd = NewQueue->BufferBegin + QueueLength * ItemSize;

        NewQueue->Head = NewQueue->BufferBegin;
        NewQueue->Tail = NewQueue->BufferBegin;
    }

    return NewQueue;
}

void MyQueueDelete(MyQueueHandle_t MyQueue) {
    configASSERT(MyQueue);

    configASSERT(MyQueue->EmptySemaphore);
    configASSERT(MyQueue->FullSemaphore);
    configASSERT(MyQueue->ModifySemaphore);

    vMySemaphoreDelete(MyQueue->EmptySemaphore);
    vMySemaphoreDelete(MyQueue->FullSemaphore);
    vMySemaphoreDelete(MyQueue->ModifySemaphore);
}

BaseType_t MyQueueSendToBack(MyQueueHandle_t MyQueue,
                             const void* ItemToQueue,
                             TickType_t TicksToWait)
{
    // take EmptySemaphore to get an empty slot to push an item
    if (xMySemaphoreTake(MyQueue->EmptySemaphore, TicksToWait / 2) == pdTRUE) {
        // get mutual exclusive access to ModifySemaphore to write
        if (xMySemaphoreTake(MyQueue->ModifySemaphore, TicksToWait / 2) == pdTRUE) {
            // write to Tail and then increment Tail
            memcpy((void*) MyQueue->Tail, ItemToQueue, MyQueue->ItemSize);

            MyQueue->Tail += MyQueue->ItemSize;
            if (MyQueue->Tail == MyQueue->BufferEnd) {
                MyQueue->Tail = MyQueue->BufferBegin;
            }

            // done writing
            xMySemaphoreGive(MyQueue->ModifySemaphore, portMAX_DELAY);

            // one more slot of the queue is now full
            xMySemaphoreGive(MyQueue->FullSemaphore, portMAX_DELAY);

            return pdTRUE;
        } else {
            // we were unsuccessful getting ModifySemaphore so give back the
            // empty slot we took
            xMySemaphoreGive(MyQueue->EmptySemaphore, portMAX_DELAY);
        }
    }

    return errQUEUE_FULL;
}

BaseType_t MyQueueReceive(MyQueueHandle_t MyQueue,
                          void* Buffer,
                          TickType_t TicksToWait)
{
    // take FullSemaphore to get a full slot to pop an item
    if (xMySemaphoreTake(MyQueue->FullSemaphore, TicksToWait / 2) == pdTRUE) {
        // get mutual exclusive access to ModifySemaphore to read
        if (xMySemaphoreTake(MyQueue->ModifySemaphore, TicksToWait / 2) == pdTRUE) {
            // read from Head and then increment Head
            memcpy(Buffer, (void*) MyQueue->Head, MyQueue->ItemSize);

            MyQueue->Head += MyQueue->ItemSize;
            if (MyQueue->Head == MyQueue->BufferEnd) {
                MyQueue->Head = MyQueue->BufferBegin;
            }

            // done reading
            xMySemaphoreGive(MyQueue->ModifySemaphore, portMAX_DELAY);

            // one more slot of the queue is now empty
            xMySemaphoreGive(MyQueue->EmptySemaphore, portMAX_DELAY);

            return pdTRUE;
        } else {
            // we were unsuccessful getting ModifySemaphore so give back the
            // full slot we took
            xMySemaphoreGive(MyQueue->FullSemaphore, portMAX_DELAY);
        }
    }

    return errQUEUE_EMPTY;
}

BaseType_t MyQueueSendToBackFromISR(MyQueueHandle_t MyQueue,
                                    const void* ItemToQueue,
                                    BaseType_t* HigherPriorityTaskWoken)
{
    BaseType_t Status = errQUEUE_FULL;

    // attempt to get ModifySemaphore without waiting
    // taking ModifySemaphore should not awaken any givers since the giver of
    // ModifySemaphore is always the taker so we should never have any
    // processes waiting to give so HigherPriorityTaskWoken parameter is
    // irrelevant here
    if (xMySemaphoreTakeFromISR(MyQueue->ModifySemaphore, NULL) == pdTRUE) {
        // We can assume that what happens in this block occurs atomically
        // since only other interrupts with a higher priority can interrupt
        // this function but they would not be able to acquire the ModifySmepahore

        // take and give functions may notify other tasks so we should not call
        // take and give unless we are confident they will succeed which we can
        // quickly check since we are not waiting
        if (xMySemaphoreTakeAvailableFromISR(MyQueue->EmptySemaphore) == pdTRUE &&
            xMySemaphoreGiveAvailableFromISR(MyQueue->FullSemaphore) == pdTRUE)
        {
            BaseType_t EmptyWoken = pdFALSE;
            BaseType_t FullWoken = pdFALSE;

            // take EmptySemaphore to push an item and ensure it succeeds
            configASSERT(xMySemaphoreTakeFromISR(MyQueue->EmptySemaphore,
                                                &EmptyWoken) == pdTRUE);

            // write to Tail and then increment Tail
            memcpy((void*) MyQueue->Tail, ItemToQueue, MyQueue->ItemSize);

            MyQueue->Tail += MyQueue->ItemSize;
            if (MyQueue->Tail == MyQueue->BufferEnd) {
                MyQueue->Tail = MyQueue->BufferBegin;
            }

            // give FullSemaphore to indicate an item has been pushed and
            // ensure it succeeds
            configASSERT(xMySemaphoreGiveFromISR(MyQueue->FullSemaphore,
                                                &FullWoken) == pdTRUE);

            // Set HigherPriorityTaskWoken indicator here if not NULL
            if (HigherPriorityTaskWoken != NULL) {
                *HigherPriorityTaskWoken =
                    ((EmptyWoken == pdTRUE) || (FullWoken == pdTRUE)) ? pdTRUE
                                                                      : pdFALSE;
            }

            Status = pdTRUE;
        }

        // done modifying, this should always succeed since we guarantee hold
        // the ModifySemaphore
        xMySemaphoreGiveFromISR(MyQueue->ModifySemaphore, NULL);
    }

    return Status;
}

BaseType_t MyQueueReceiveFromISR(MyQueueHandle_t MyQueue,
                                 void* Buffer,
                                 BaseType_t* HigherPriorityTaskWoken)
{
    BaseType_t Status = errQUEUE_EMPTY;

    // attempt to get ModifySemaphore without waiting
    // taking ModifySemaphore should not awaken any givers since the giver of
    // ModifySemaphore is always the taker so we should never have any
    // processes waiting to give so HigherPriorityTaskWoken parameter is
    // irrelevant here
    if (xMySemaphoreTakeFromISR(MyQueue->ModifySemaphore, NULL) == pdTRUE) {
        // We can assume that what happens in this block occurs atomically
        // since only other interrupts with a higher priority can interrupt
        // this function but they would not be able to acquire the ModifySmepahore

        // take and give functions may notify other tasks so we should not call
        // take and give unless we are confident they will succeed which we can
        // quickly check since we are not waiting
        if (xMySemaphoreTakeAvailableFromISR(MyQueue->FullSemaphore) == pdTRUE &&
            xMySemaphoreGiveAvailableFromISR(MyQueue->EmptySemaphore) == pdTRUE)
        {
            BaseType_t FullWoken = pdFALSE;
            BaseType_t EmptyWoken = pdFALSE;

            // take FullSemaphore to pop an item and ensure it succeeds
            configASSERT(xMySemaphoreTakeFromISR(MyQueue->FullSemaphore,
                                                &FullWoken) == pdTRUE);

            // read from Head and then increment Head
            memcpy(Buffer, (void*) MyQueue->Head, MyQueue->ItemSize);

            MyQueue->Head += MyQueue->ItemSize;
            if (MyQueue->Head == MyQueue->BufferEnd) {
                MyQueue->Head = MyQueue->BufferBegin;
            }

            // give EmptySemaphore to indicate an item has been popped and
            // ensure it succeeds
            configASSERT(xMySemaphoreGiveFromISR(MyQueue->EmptySemaphore,
                                                &EmptyWoken) == pdTRUE);

            // Set HigherPriorityTaskWoken indicator here if not NULL
            if (HigherPriorityTaskWoken != NULL) {
                *HigherPriorityTaskWoken =
                    ((FullWoken == pdTRUE) || (EmptyWoken == pdTRUE)) ? pdTRUE
                                                                      : pdFALSE;
            }

            Status = pdTRUE;
        }

        // done modifying, this should always succeed since we guarantee hold
        // the ModifySemaphore
        xMySemaphoreGiveFromISR(MyQueue->ModifySemaphore, NULL);
    }

    return Status;
}
