#ifndef SERTOS_TASK_CONSUMER_H
#define SERTOS_TASK_CONSUMER_H

/**
 * @brief Consumer task entry point.
 *
 * @param param Pointer to the demo queue handle.
 */
__attribute__((no_instrument_function)) void sertos_task_consumer(void* param);

#endif /* SERTOS_TASK_CONSUMER_H */
