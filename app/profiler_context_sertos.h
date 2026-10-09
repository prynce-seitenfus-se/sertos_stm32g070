#ifndef PROFILER_CONTEXT_SERTOS_H
#define PROFILER_CONTEXT_SERTOS_H

#include "sertos_types.h"

/**
 * @brief SerTOS context-switch hook that forwards switches to the profiler.
 *
 * Register it as SertosConfig::switch_hook. It records the incoming task as the
 * current profiler context (read back by profiler_port_context_id()) and calls
 * profiler_context_switch(). Runs with interrupts masked (PendSV); it is never
 * instrumented and does not call any instrumented SerTOS API.
 *
 * @param prev Task being switched out (NULL on the first switch).
 * @param next Task being switched in.
 */
void profiler_context_sertos_switch_hook(SertosTaskHandle prev, SertosTaskHandle next);

#endif /* PROFILER_CONTEXT_SERTOS_H */
