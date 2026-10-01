#include "sertos_task_consumer.h"

#include "gpio.h"
#include "sertos_demo_queue.h"
#include "sertos_types.h"

void sertos_task_consumer(void* param)
{
    SertosDemoQueue* queue = (SertosDemoQueue*)param;
    SertosDemoQueueItem item;

    while (1) {
        if (sertos_demo_queue_receive(queue, &item, SERTOS_WAIT_FOREVER) == SERTOS_STATUS_OK) {
            HAL_GPIO_WritePin(LED_GREEN_GPIO_Port,
                              LED_GREEN_Pin,
                              ((item.value % 100U) < 50U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
    }
}
