#include "FreeRTOS.h"
#include "task.h"
#include "my_semaphore.h"
#include "my_queue.h"

#include <stdio.h>

void main_my_queue(void) {
    MyQueueHandle_t queue = MyQueueCreate();

    printf("%s\n", queue == NULL ? "null" : "gucci");
}
