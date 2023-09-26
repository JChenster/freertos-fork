#include "FreeRTOS.h"

#include "my_semaphore.h"
#include "task.h"

// TODO: is this necessary?
/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
 * all the API functions to use the MPU wrappers.  That should only be done when
 * task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

struct MySemaphoreDefinition {
    List_t InternalList;
};

MySemaphoreHandle_t MySemaphoreCreate(const UBaseType_t MaxCount,
                                      const UBaseType_t InitialCount) {
    // TODO: Do input boundary checks

    MySemaphoreHandle_t NewSemaphore = pvPortMalloc(sizeof(MySemaphore_t));

    if (NewSemaphore == NULL) {
        // TODO: what to do when malloc fails?
        return NULL;
    }

    vListInitialise(&(NewSemaphore->InternalList));

    return NewSemaphore;
}
