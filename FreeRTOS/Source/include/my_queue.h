#ifndef MYQUEUE_H
#define MYQUEUE_H

#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h" must appear in source files before \
           "include my_queue.h"
#endif

// Opague struct definition
typedef struct MyQueueDefinition MyQueue_t;
typedef MyQueue_t* MyQueueHandle_t;

MyQueueHandle_t pxMyQueueCreate( UBaseType_t xQueueLength, UBaseType_t xItemSize );

void vMyQueueDelete( MyQueueHandle_t pxMyQueue );

BaseType_t xMyQueueSendToBack( MyQueueHandle_t pxMyQueue,
                               const void* pvItemToQueue,
                               TickType_t xTicksToWait );

BaseType_t xMyQueueReceive( MyQueueHandle_t pxMyQueue,
                            void* pvBuffer,
                            TickType_t xTicksToWait );

BaseType_t xMyQueueSendToBackFromISR( MyQueueHandle_t pxMyQueue,
                                      const void* pvItemToQueue,
                                      BaseType_t* pxHigherPriorityTaskWoken );

BaseType_t xMyQueueReceiveFromISR( MyQueueHandle_t pxMyQueue,
                                   void* pvBuffer,
                                   BaseType_t* pxHigherPriorityTaskWoken );

#endif // MYQUEUE_H
