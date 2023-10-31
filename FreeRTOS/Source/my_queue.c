#include "FreeRTOS.h"

#include "my_queue.h"
#include "my_semaphore.h"

struct MyQueueDefinition {
    MySemaphoreHandle_t Semaphore;
};

MyQueueHandle_t MyQueueCreate() {
    MyQueueHandle_t NewQueue = pvPortMalloc(sizeof(MyQueue_t));

    if (NewQueue == NULL) {
        return NULL;
    }

    return NewQueue;
}
