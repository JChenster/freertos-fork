#include "FreeRTOS.h"

#include "my_queue.h"
#include "my_semaphore.h"

#include <string.h>

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

BaseType_t MyQueueSendToBack(MyQueueHandle_t MyQueue, const void* ItemToQueue) {
    // Assume we wait indefinitely for send to succeed
    for (;;) {
        // get access to ItemSemaphore to add an item and
        // get mutual exclusive access to ModifySemaphore to write
        if (MySemaphoreGive(MyQueue->ItemSemaphore) == pdTRUE &&
            MySemaphoreTake(MyQueue->ModifySemaphore, portMAX_DELAY) == pdTRUE)
        {
            // write to Tail and then increment Tail
            memcpy((void*) MyQueue->Tail, ItemToQueue, MyQueue->ItemSize);

            MyQueue->Tail += MyQueue->ItemSize;
            if (MyQueue->Tail == MyQueue->BufferEnd) {
                MyQueue->Tail = MyQueue->BufferBegin;
            }

            // done writing
            MySemaphoreGive(MyQueue->ModifySemaphore);
            return pdTRUE;
        }
    }

    return errQUEUE_FULL;
}

BaseType_t MyQueueReceive(MyQueueHandle_t MyQueue, void* Buffer) {
    // Assume we wait indefinitely for send to succeed
    for (;;) {
        // get access to ItemSemaphore to pop an item and
        // get mutual exclusive access to ModifySemaphore to read
        if (MySemaphoreTake(MyQueue->ItemSemaphore, portMAX_DELAY) == pdTRUE &&
            MySemaphoreTake(MyQueue->ModifySemaphore, portMAX_DELAY) == pdTRUE)
        {
            // read from Head and then increment Head
            memcpy(Buffer, (void*) MyQueue->Head, MyQueue->ItemSize);

            MyQueue->Head += MyQueue->ItemSize;
            if (MyQueue->Head == MyQueue->BufferEnd) {
                MyQueue->Head = MyQueue->BufferBegin;
            }

            // done reading
            MySemaphoreGive(MyQueue->ModifySemaphore);
            return pdTRUE;
        }
    }

    return errQUEUE_EMPTY;
}
