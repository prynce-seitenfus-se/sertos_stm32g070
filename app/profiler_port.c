#include "profiler_port.h"
#include "tim.h"

void profiler_port_init(void)
{
	__HAL_TIM_SET_COUNTER(&htim1, 0U);
	__HAL_TIM_SET_COUNTER(&htim3, 0U);

	if ( HAL_TIM_Base_Start(&htim3) != HAL_OK ) { // slave 1st
        Error_Handler();        
	}
	else if ( HAL_TIM_Base_Start(&htim1) != HAL_OK ) { // then master
		Error_Handler();
	}
	else {
        // Both timers running: TIM1 = low 16 bits, TIM3 = high 16 bits
	}
}

uint32_t profiler_port_ticks(void)
{
	uint32_t hi = __HAL_TIM_GET_COUNTER(&htim3);
	uint32_t lo = __HAL_TIM_GET_COUNTER(&htim1); // warning: interrupt may occur
	const uint32_t hi2 = __HAL_TIM_GET_COUNTER(&htim3);

	if ( hi != hi2 )
	{
		/* TIM1 wrapped between the reads: re-read the low half so it belongs to hi2 */
		lo = __HAL_TIM_GET_COUNTER(&htim1);
		hi = hi2;
	}

	return ( hi << 16U ) | ( lo & 0xFFFFU );
}
