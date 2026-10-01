#ifndef SERTOS_DEMO_QUEUE_H
#define SERTOS_DEMO_QUEUE_H

#include <stdint.h>

#include "sertos_types.h"

/**
 * @brief Opaque handle for the statically allocated demo queue.
 */
typedef struct SertosDemoQueue SertosDemoQueue;

/**
 * @brief Queue item carrying the producer sequence value.
 */
typedef struct SertosDemoQueueItem {
    uint32_t value;
} SertosDemoQueueItem;

/**
 * @brief Initializes the statically allocated demo queue.
 *
 * @param[out] out_queue Receives the opaque queue handle.
 * @return SERTOS_STATUS_OK on success, or an error status on failure.
 */
SertosStatus sertos_demo_queue_init(SertosDemoQueue** out_queue);

/**
 * @brief Sends an item to the demo queue.
 *
 * @param[in] queue Queue handle.
 * @param[in] item Item to send.
 * @param[in] timeout Maximum ticks to wait.
 * @return SERTOS_STATUS_OK on success, or an error status on failure.
 */
SertosStatus sertos_demo_queue_send(SertosDemoQueue* queue,
                                    const SertosDemoQueueItem* item,
                                    SertosTick timeout);

/**
 * @brief Receives an item from the demo queue.
 *
 * @param[in] queue Queue handle.
 * @param[out] item Receives the dequeued item.
 * @param[in] timeout Maximum ticks to wait.
 * @return SERTOS_STATUS_OK on success, or an error status on failure.
 */
SertosStatus sertos_demo_queue_receive(SertosDemoQueue* queue,
                                       SertosDemoQueueItem* item,
                                       SertosTick timeout);

#endif /* SERTOS_DEMO_QUEUE_H */
