#include "profiler_context_sertos.h"

#include <stddef.h>
#include <stdint.h>

#include "main.h"
#include "profiler.h"
#include "profiler_port.h"

/*
 * Task owning the CPU as last reported by the scheduler switch hook. NULL
 * before the first switch, so pre-scheduler code shares the NULL context.
 * Cached here because sertos_scheduler_get_current_tcb() is instrumented in
 * the instrumented SerTOS library and would recurse into the profiler hooks.
 */
static const void* volatile s_current_task = NULL;

NO_INST const void* profiler_port_context_id(void)
{
    if (__get_IPSR() != 0U) {
        return PROFILER_CONTEXT_ISR;
    }

    return s_current_task;
}

NO_INST void profiler_context_sertos_switch_hook(SertosTaskHandle prev, SertosTaskHandle next)
{
    s_current_task = (const void*)next;
    profiler_context_switch((const void*)prev, (const void*)next);
}
