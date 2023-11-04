#ifndef MYSEMAPHORE_H
#define MYSEMAPHORE_H

 #ifndef INC_FREERTOS_H
     #error "include FreeRTOS.h" must appear in source files before "include queue.h"
 #endif

typedef struct MySemaphoreDefinition MySemaphore_t;
typedef MySemaphore_t* MySemaphoreHandle_t;

MySemaphoreHandle_t MySemaphoreCreate(const UBaseType_t MaxCount,
                                      const UBaseType_t InitialCount);

BaseType_t MySemaphoreTake(MySemaphoreHandle_t MySemaphore,
                           TickType_t TicksToWait);

BaseType_t MySemaphoreGive(MySemaphoreHandle_t MySemaphoreHandle);

#endif // MYSEMAPHORE_H
