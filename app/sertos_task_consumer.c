#include "sertos_task_consumer.h"

#include "gpio.h"
#include "profiler.h"
#include "sertos_demo_queue.h"
#include "sertos_types.h"

void sertos_task_consumer(void* param)
{
    SertosDemoQueue* queue = (SertosDemoQueue*)param;
    SertosDemoQueueItem item;
    SertosStatus status;

    while (1) {
        PROFILER_SCOPE(sertos_demo_queue_receive,
                       status = sertos_demo_queue_receive(queue, &item, SERTOS_WAIT_FOREVER));
        if (status == SERTOS_STATUS_OK) {
            HAL_GPIO_WritePin(LED_GREEN_GPIO_Port,
                              LED_GREEN_Pin,
                              ((item.value % 100U) < 50U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
    }
}
