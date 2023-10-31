#ifndef MYQUEUE_H
#define MYQUEUE_H

#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h" must appear in source files before \
           "include my_queue.h"
#endif

// Opague struct definition
typedef struct MyQueueDefinition MyQueue_t;
typedef MyQueue_t* MyQueueHandle_t;

MyQueueHandle_t MyQueueCreate();

#endif // MYQUEUE_H
