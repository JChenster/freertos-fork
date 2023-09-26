/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"

#include "my_semaphore.h"

void main_my_sem(void) {
    MySemaphoreHandle_t MySemaphore = MySemaphoreCreate(2, 0);
    printf("Success!\n");
}
