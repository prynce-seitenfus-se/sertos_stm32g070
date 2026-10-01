#ifndef SERTOS_TASK_PROFILER_H
#define SERTOS_TASK_PROFILER_H

#include <stdbool.h>

/**
 * @brief Initializes and starts the profiler event capture.
 */
void sertos_task_profiler_init(void);

/**
 * @brief Profiler task entry point.
 *
 * @param param Unused task argument.
 */
void sertos_task_profiler(void* param);

/**
 * @brief Starts receiving profiler commands over USART2.
 *
 * @return true if DMA receive started successfully; otherwise false.
 */
bool sertos_task_profiler_start_uart_receive(void);

#endif /* SERTOS_TASK_PROFILER_H */
