#include "main.h"
#include "sertos_port.h"

uint32_t sertos_port_tick_clock_hz(void)
{
    return SystemCoreClock;
}
