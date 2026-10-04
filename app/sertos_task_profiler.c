#include "sertos_task_profiler.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "main.h"
#include "profiler.h"
#include "profiler_transport.h"
#include "sertos_scheduler.h"
#include "usart.h"

#define PROFILER_MAP_CAPACITY     (64U)
#define PROFILER_STACK_DEPTH      (32U)
#define PROFILER_TX_BUFFER_SIZE   (256U)
#define PROFILER_RX_BUFFER_SIZE   (32U)
#define PROFILER_TX_TIMEOUT_MS    (1000U)
#define PROFILER_DUMP_COMMAND     "prof-dump"

static HashMapEntry s_profiler_map[PROFILER_MAP_CAPACITY];
static ProfilerMetric s_profiler_metrics[PROFILER_MAP_CAPACITY];
static ProfilerStackFrame s_profiler_stack[PROFILER_STACK_DEPTH];
static uint8_t s_profiler_tx_buffer[PROFILER_TX_BUFFER_SIZE] __attribute__((aligned(32)));
static uint8_t s_profiler_rx_buffer[PROFILER_RX_BUFFER_SIZE] __attribute__((aligned(32)));

static __attribute__((no_instrument_function)) bool profiler_command_matches(const uint8_t* buffer, size_t length);
static __attribute__((no_instrument_function)) bool profiler_dump_metrics(void);
static __attribute__((no_instrument_function)) uint32_t profiler_tick_ms(void);
static __attribute__((no_instrument_function)) void profiler_yield_ms(uint32_t milliseconds);

void sertos_task_profiler_init(void)
{
    ProfilerConfig profiler_config = {
        .frequency = HAL_RCC_GetPCLK1Freq(),
        .map_entries = s_profiler_map,
        .map_capacity = PROFILER_MAP_CAPACITY,
        .metrics = s_profiler_metrics,
        .metrics_capacity = PROFILER_MAP_CAPACITY,
        .stack_frames = s_profiler_stack,
        .stack_depth = PROFILER_STACK_DEPTH
    };
    ProfilerTransportConfig transport_config = {
        .device = &huart2,
        .tx_buffer = s_profiler_tx_buffer,
        .tx_buffer_size = sizeof(s_profiler_tx_buffer),
        .rx_buffer = s_profiler_rx_buffer,
        .rx_buffer_size = sizeof(s_profiler_rx_buffer),
        .tx_timeout_ms = PROFILER_TX_TIMEOUT_MS,
        .tick_ms = profiler_tick_ms,
        .yield = profiler_yield_ms
    };

    if (!profiler_init(&profiler_config)) {
        Error_Handler();
    }
    if (profiler_transport_init(&transport_config) != PROFILER_TRANSPORT_STATUS_OK) {
        Error_Handler();
    }
    if (profiler_transport_listen() != PROFILER_TRANSPORT_STATUS_OK) {
        Error_Handler();
    }

    profiler_start();
}

void sertos_task_profiler(void* param)
{
    (void)param;

    while (1) {
        size_t frame_length = 0U;
        uint8_t events = profiler_transport_poll(&frame_length);

        if ((events & PROFILER_TRANSPORT_EVENT_ERROR) != 0U) {
            (void)profiler_transport_status();
            Error_Handler();
        }
        if ((events & PROFILER_TRANSPORT_EVENT_OVERRUN) != 0U) {
            Error_Handler();
        }
        if ((events & PROFILER_TRANSPORT_EVENT_FRAME) != 0U) {
            if (profiler_command_matches(s_profiler_rx_buffer, frame_length) &&
                !profiler_dump_metrics()) {
                Error_Handler();
            }
            if (profiler_transport_listen() != PROFILER_TRANSPORT_STATUS_OK) {
                Error_Handler();
            }
        }

        (void)sertos_scheduler_delay(10U);
    }
}

static bool profiler_command_matches(const uint8_t* buffer, size_t length)
{
    size_t first = 0U;
    size_t last = length;
    size_t command_length = sizeof(PROFILER_DUMP_COMMAND) - 1U;

    if ((buffer == NULL) || (length > PROFILER_RX_BUFFER_SIZE)) {
        return false;
    }

    while ((first < last) &&
           ((buffer[first] == (uint8_t)' ') || (buffer[first] == (uint8_t)'\t') ||
            (buffer[first] == (uint8_t)'\r') || (buffer[first] == (uint8_t)'\n'))) {
        first++;
    }
    while ((last > first) &&
           ((buffer[last - 1U] == (uint8_t)' ') || (buffer[last - 1U] == (uint8_t)'\t') ||
            (buffer[last - 1U] == (uint8_t)'\r') || (buffer[last - 1U] == (uint8_t)'\n'))) {
        last--;
    }

    if ((last - first) != command_length) {
        return false;
    }

    for (size_t index = 0U; index < command_length; index++) {
        if (buffer[first + index] != (uint8_t)PROFILER_DUMP_COMMAND[index]) {
            return false;
        }
    }

    return true;
}

static bool profiler_dump_metrics(void)
{
    const Stream* stream = profiler_transport_stream();
    bool success;

    if (stream == NULL) {
        return false;
    }

    profiler_stop();
    (void)profiler_transport_status();
    success = profiler_dump(stream);
    if (profiler_transport_status() != PROFILER_TRANSPORT_STATUS_OK) {
        success = false;
    }
    profiler_start();

    return success;
}

static uint32_t profiler_tick_ms(void)
{
    return (uint32_t)sertos_scheduler_get_tick_count();
}

static void profiler_yield_ms(uint32_t milliseconds)
{
    (void)sertos_scheduler_delay((SertosTick)milliseconds);
}
