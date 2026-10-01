#include "profiler_port.h"
#include "sertos_port.h"
#include "sertos_scheduler.h"

uint32_t __atomic_fetch_add_4(volatile void* pointer, uint32_t value, int memory_order)
{
    volatile uint32_t* target = (volatile uint32_t*)pointer;
    uint32_t critical_state;
    uint32_t previous;

    (void)memory_order;
    critical_state = sertos_port_enter_critical();
    previous = *target;
    *target = previous + value;
    sertos_port_exit_critical(critical_state);

    return previous;
}

void profiler_port_init(void)
{
}

uint32_t profiler_port_ticks(void)
{
    return (uint32_t)sertos_scheduler_get_tick_count();
}
