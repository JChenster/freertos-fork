#include "FreeRTOS.h"
#include "task.h"
#include "my_semaphore.h"
#include "my_queue.h"

#include <stdio.h>

void main_my_queue(void) {
    MyQueueHandle_t queue = MyQueueCreate(10, sizeof(int));
    printf("%s\n", queue == NULL ? "null" : "gucci");

    int x = 5;
    MyQueueSendToBack(queue, &x);
    printf("send successful\n");

    int y;
    MyQueueReceive(queue, &y);
    printf("y = %d (should be 5)\n", y);
}
