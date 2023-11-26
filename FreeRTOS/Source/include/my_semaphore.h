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

BaseType_t MySemaphoreGive(MySemaphoreHandle_t MySemaphoreHandle,
                           TickType_t TicksToWait);

BaseType_t MySemaphoreTakeFromISR(MySemaphoreHandle_t MySemaphore,
                                  BaseType_t* HigherPriorityTaskWoken);

BaseType_t MySemaphoreGiveFromISR(MySemaphoreHandle_t MySemaphore,
                                  BaseType_t* HigherPriorityTaskWoken);

BaseType_t MySemaphoreTakeAvailableFromISR(MySemaphoreHandle_t MySemaphore);

BaseType_t MySemaphoreGiveAvailableFromISR(MySemaphoreHandle_t MySemaphore);

#endif // MYSEMAPHORE_H
