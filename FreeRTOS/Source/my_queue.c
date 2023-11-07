#include "FreeRTOS.h"

#include "my_queue.h"
#include "my_semaphore.h"

#include <string.h>
#include <stdio.h>

struct MyQueueDefinition {
    // Synchronization
    MySemaphoreHandle_t ItemSemaphore;
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

        // create a semaphore whose count represents how many items are on the
        // queue
        NewQueue->ItemSemaphore = MySemaphoreCreate(QueueLength, 0);
        // create a binary semaphore that will allow mutual exclusivity while
        // reading / writing
        NewQueue->ModifySemaphore = MySemaphoreCreate(1, 1);

        if (NewQueue->ItemSemaphore == NULL ||
            NewQueue->ModifySemaphore == NULL)
        {
            // TODO: gracefully delete
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

BaseType_t MyQueueSendToBack(MyQueueHandle_t MyQueue,
                             const void* ItemToQueue,
                             TickType_t TicksToWait)
{
    // get access to ItemSemaphore to push an item
    if (MySemaphoreGive(MyQueue->ItemSemaphore, TicksToWait / 2) == pdTRUE) {
        // get mutual exclusive access to ModifySemaphore to write
        if (MySemaphoreTake(MyQueue->ModifySemaphore, TicksToWait / 2) == pdTRUE) {
            // write to Tail and then increment Tail
            memcpy((void*) MyQueue->Tail, ItemToQueue, MyQueue->ItemSize);

            MyQueue->Tail += MyQueue->ItemSize;
            if (MyQueue->Tail == MyQueue->BufferEnd) {
                MyQueue->Tail = MyQueue->BufferBegin;
            }

            // done writing
            MySemaphoreGive(MyQueue->ModifySemaphore, portMAX_DELAY);
            return pdTRUE;
        } else {
            // we were unsuccessful getting ModifySemaphore so undo
            // ItemSemaphore Give. Set TicksToWait as portMAX_DELAY since we
            // always want this operation to succeed
            MySemaphoreTake(MyQueue->ItemSemaphore, portMAX_DELAY);
        }
    }

    return errQUEUE_FULL;
}

BaseType_t MyQueueReceive(MyQueueHandle_t MyQueue,
                          void* Buffer,
                          TickType_t TicksToWait)
{
    // get access to ItemSemaphore to pop an item
    if (MySemaphoreTake(MyQueue->ItemSemaphore, TicksToWait / 2) == pdTRUE) {
        // get mutual exclusive access to ModifySemaphore to read
        if (MySemaphoreTake(MyQueue->ModifySemaphore, TicksToWait / 2) == pdTRUE) {
            // read from Head and then increment Head
            memcpy(Buffer, (void*) MyQueue->Head, MyQueue->ItemSize);

            MyQueue->Head += MyQueue->ItemSize;
            if (MyQueue->Head == MyQueue->BufferEnd) {
                MyQueue->Head = MyQueue->BufferBegin;
            }

            // done reading
            MySemaphoreGive(MyQueue->ModifySemaphore, portMAX_DELAY);
            return pdTRUE;
        } else {
            // we were unsuccessful getting ModifySemaphore so undo
            // ItemSemaphore Take. Set TicksToWait as portMAX_DELAY since we
            // always want this operation to succeed
            MySemaphoreGive(MyQueue->ItemSemaphore, portMAX_DELAY);
        }
    }

    return errQUEUE_EMPTY;
}

BaseType_t MyQueueSendToBackFromISR(MyQueueHandle_t MyQueue,
                                    const void* ItemToQueue,
                                    BaseType_t* HigherPriorityTaskWoken)
{
    BaseType_t ret = errQUEUE_FULL;

    // attempt to get ModifySemaphore without waiting
    // taking ModifySemaphore should not awaken any givers since the giver of
    // ModifySemaphore is always the taker so we should never have any
    // processes waiting to give so HigherPriorityTaskWoken parameter is
    // irrelevant here
    if (MySemaphoreTakeFromISR(MyQueue->ModifySemaphore, NULL) == pdTRUE) {
        // Attempt to give semaphore to push an item
        BaseType_t SemHigherPriorityTaskWoken;
        if (MySemaphoreGiveFromISR(MyQueue->ItemSemaphore,
                                   &SemHigherPriorityTaskWoken) == pdTRUE)
        {
            // write to Tail and then increment Tail
            memcpy((void*) MyQueue->Tail, ItemToQueue, MyQueue->ItemSize);

            MyQueue->Tail += MyQueue->ItemSize;
            if (MyQueue->Tail == MyQueue->BufferEnd) {
                MyQueue->Tail = MyQueue->BufferBegin;
            }

            ret = pdTRUE;

            // a task may be woken from giving to ItemSemaphore
            if (HigherPriorityTaskWoken != NULL) {
                *HigherPriorityTaskWoken = SemHigherPriorityTaskWoken;
            }
        }
        // done modifying, this should always succeed
        MySemaphoreGiveFromISR(MyQueue->ModifySemaphore, NULL);
    }

    return ret;
}
