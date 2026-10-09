#include "sertos_task_producer.h"

#include <stdint.h>

#include "gpio.h"
#include "profiler.h"
#include "sertos_demo_queue.h"
#include "sertos_scheduler.h"
#include "sertos_types.h"

void sertos_task_producer(void* param)
{
    SertosDemoQueue* queue = (SertosDemoQueue*)param;
    SertosDemoQueueItem item;
    static uint32_t sequence = 0U;
    uint8_t state = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) ? 1U : 0U;
    uint8_t count = 0U;
    uint8_t paused = 0U;

    while (1) {
        uint8_t raw = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) ? 1U : 0U;

        if (raw != state) {
            count++;
            if (count >= 4U) {
                state = raw;
                count = 0U;
                if (state == 0U) {
                    paused ^= 1U;
                }
            }
        } else {
            count = 0U;
        }

        if (paused == 0U) {
            item.value = ++sequence;
            PROFILER_SCOPE(sertos_demo_queue_send, (void)sertos_demo_queue_send(queue, &item, SERTOS_NO_WAIT));
        }

        PROFILER_SCOPE(sertos_scheduler_delay_ms, (void)sertos_scheduler_delay_ms(10U));
    }
}
