#ifndef MYSEMAPHORE_H
#define MYSEMAPHORE_H

 #ifndef INC_FREERTOS_H
     #error "include FreeRTOS.h" must appear in source files before "include queue.h"
 #endif

typedef struct MySemaphoreDefinition MySemaphore_t;
typedef MySemaphore_t* MySemaphoreHandle_t;

MySemaphoreHandle_t pxMySemaphoreCreate( const UBaseType_t uxMaxCount,
                                         const UBaseType_t uxInitialCount );

void vMySemaphoreDelete( MySemaphoreHandle_t pxMySemaphore );

BaseType_t xMySemaphoreTake( MySemaphoreHandle_t pxMySemaphore,
                             TickType_t xTicksToWait );

BaseType_t xMySemaphoreGive( MySemaphoreHandle_t pxMySemaphore,
                             TickType_t xTicksToWait );

BaseType_t xMySemaphoreTakeFromISR( MySemaphoreHandle_t pxMySemaphore,
                                    BaseType_t* pxHigherPriorityTaskWoken );

BaseType_t xMySemaphoreGiveFromISR( MySemaphoreHandle_t pxMySemaphore,
                                    BaseType_t* pxHigherPriorityTaskWoken );

BaseType_t xMySemaphoreTakeAvailableFromISR( MySemaphoreHandle_t pxMySemaphore );

BaseType_t xMySemaphoreGiveAvailableFromISR( MySemaphoreHandle_t pxMySemaphore );

#endif // MYSEMAPHORE_H
