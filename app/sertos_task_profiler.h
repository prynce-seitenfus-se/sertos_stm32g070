#ifndef SERTOS_TASK_PROFILER_H
#define SERTOS_TASK_PROFILER_H

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

#endif /* SERTOS_TASK_PROFILER_H */
