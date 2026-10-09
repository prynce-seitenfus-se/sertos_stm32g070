#include "sertos_demo_queue.h"

#include <stddef.h>

#include "profiler.h"
#include "sertos_queue.h"

#define SERTOS_DEMO_QUEUE_LENGTH (8U)

struct SertosDemoQueue {
    SertosQueue queue;
    SertosQueueHandle handle;
    SertosDemoQueueItem storage[SERTOS_DEMO_QUEUE_LENGTH];
};

static struct SertosDemoQueue s_demo_queue;

SertosStatus sertos_demo_queue_init(SertosDemoQueue** out_queue)
{
    SertosStatus status;

    if (out_queue == NULL) {
        return SERTOS_STATUS_ERROR_NULL_PTR;
    }

    *out_queue = NULL;
    PROFILER_SCOPE(sertos_queue_create_static,
                   status = sertos_queue_create_static(&s_demo_queue.queue,
                                                       s_demo_queue.storage,
                                                       sizeof(s_demo_queue.storage),
                                                       sizeof(SertosDemoQueueItem),
                                                       &s_demo_queue.handle));
    if (status == SERTOS_STATUS_OK) {
        *out_queue = &s_demo_queue;
    }

    return status;
}

SertosStatus sertos_demo_queue_send(SertosDemoQueue* queue,
                                    const SertosDemoQueueItem* item,
                                    SertosTick timeout)
{
    SertosStatus status;

    if ((queue == NULL) || (item == NULL)) {
        return SERTOS_STATUS_ERROR_NULL_PTR;
    }

    PROFILER_SCOPE(sertos_queue_send, status = sertos_queue_send(queue->handle, item, timeout));
    return status;
}

SertosStatus sertos_demo_queue_receive(SertosDemoQueue* queue,
                                       SertosDemoQueueItem* item,
                                       SertosTick timeout)
{
    SertosStatus status;

    if ((queue == NULL) || (item == NULL)) {
        return SERTOS_STATUS_ERROR_NULL_PTR;
    }

    PROFILER_SCOPE(sertos_queue_receive, status = sertos_queue_receive(queue->handle, item, timeout));
    return status;
}
