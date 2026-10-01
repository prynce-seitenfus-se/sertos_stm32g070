#include "sertos_task_profiler.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "main.h"
#include "profiler.h"
#include "sertos_port.h"
#include "sertos_scheduler.h"
#include "usart.h"
#include "stm32g0xx_hal_uart_ex.h"

#define PROFILER_EVENT_CAPACITY       (1024U)
#define PROFILER_COMMAND_BUFFER_SIZE  (32U)
#define PROFILER_TX_BUFFER_SIZE       (256U)
#define PROFILER_TX_TIMEOUT_MS        (1000U)
#define PROFILER_DUMP_COMMAND         "prof-dump"
#define PROFILER_DUMP_COMMAND_LENGTH  (sizeof(PROFILER_DUMP_COMMAND) - 1U)

static profiler_event_t s_profiler_events[PROFILER_EVENT_CAPACITY];
static uint8_t s_profiler_command_buffer[PROFILER_COMMAND_BUFFER_SIZE]
    __attribute__((aligned(32)));
static uint8_t s_profiler_tx_buffer[PROFILER_TX_BUFFER_SIZE]
    __attribute__((aligned(32)));
static volatile bool s_profiler_dump_requested = false;
static volatile bool s_profiler_tx_complete = false;
static volatile bool s_profiler_tx_error = false;
static volatile bool s_profiler_uart_error = false;

static bool profiler_command_matches(const uint8_t* buffer, uint16_t size)
{
    uint16_t first = 0U;
    uint16_t last = size;

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

    return ((size_t)(last - first) == PROFILER_DUMP_COMMAND_LENGTH) &&
           (memcmp(&buffer[first], PROFILER_DUMP_COMMAND, PROFILER_DUMP_COMMAND_LENGTH) == 0);
}

bool sertos_task_profiler_start_uart_receive(void)
{
    HAL_StatusTypeDef status;

    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart2,
                                          s_profiler_command_buffer,
                                          (uint16_t)sizeof(s_profiler_command_buffer));
    if (status == HAL_OK) {
        __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
        return true;
    }

    return false;
}

static bool profiler_uart_send_chunk(uint16_t length)
{
    HAL_StatusTypeDef status;
    SertosTick start_tick;

    if (length == 0U) {
        return true;
    }

    s_profiler_tx_complete = false;
    s_profiler_tx_error = false;
    status = HAL_UART_Transmit_DMA(&huart2, s_profiler_tx_buffer, length);
    if (status != HAL_OK) {
        return false;
    }

    start_tick = sertos_scheduler_get_tick_count();
    while (!s_profiler_tx_complete && !s_profiler_tx_error) {
        if ((SertosTick)(sertos_scheduler_get_tick_count() - start_tick) >=
            SERTOS_MS_TO_TICKS(PROFILER_TX_TIMEOUT_MS)) {
            (void)HAL_UART_AbortTransmit(&huart2);
            return false;
        }
        (void)sertos_scheduler_delay(1U);
    }

    return !s_profiler_tx_error;
}

static bool profiler_uart_append_line(const char* line, size_t length, size_t* buffered)
{
    if ((line == NULL) || (buffered == NULL) ||
        (*buffered > sizeof(s_profiler_tx_buffer)) ||
        (length > sizeof(s_profiler_tx_buffer))) {
        return false;
    }

    if (*buffered > (sizeof(s_profiler_tx_buffer) - length)) {
        if (!profiler_uart_send_chunk((uint16_t)*buffered)) {
            return false;
        }
        *buffered = 0U;
    }

    (void)memcpy(&s_profiler_tx_buffer[*buffered], line, length);
    *buffered += length;
    return true;
}

static bool profiler_uart_append_char(char value, size_t* buffered)
{
    return profiler_uart_append_line(&value, 1U, buffered);
}

static bool profiler_uart_append_uint32(uint32_t value, size_t* buffered)
{
    char digits[10];
    size_t count = 0U;

    do {
        digits[count] = (char)('0' + (char)(value % 10U));
        count++;
        value /= 10U;
    } while (value > 0U);

    while (count > 0U) {
        count--;
        if (!profiler_uart_append_char(digits[count], buffered)) {
            return false;
        }
    }

    return true;
}

static bool profiler_uart_append_address(uintptr_t address, size_t* buffered)
{
    static const char hexadecimal_digits[] = "0123456789abcdef";
    size_t digits = sizeof(uintptr_t) * 2U;

    if (!profiler_uart_append_line("0x", 2U, buffered)) {
        return false;
    }

    while (digits > 0U) {
        uint8_t digit;

        digits--;
        digit = (uint8_t)((address >> (digits * 4U)) & 0x0FU);
        if (!profiler_uart_append_char(hexadecimal_digits[digit], buffered)) {
            return false;
        }
    }

    return true;
}

static bool profiler_uart_dump_events(void)
{
    static const char dump_prefix[] = "# PROF-DUMP v1 events=";
    static const char frequency_field[] = " frequency_hz=";
    static const char overflow_field[] = " overflow=";
    static const char csv_header[] = "\r\nindex,timestamp,event,function,call_site\r\n";
    profiler_event_t event;
    uint32_t event_count = 0U;
    uint32_t event_index;
    uint32_t capacity;
    uint32_t overflowed;
    uint32_t critical_state;
    size_t buffered = 0U;
    bool success = true;

    profiler_stop();
    overflowed = profiler_overflowed() ? 1U : 0U;
    capacity = profiler_capacity();
    while ((event_count < capacity) && profiler_read_event(event_count, &event)) {
        event_count++;
    }

    success = profiler_uart_append_line(dump_prefix, sizeof(dump_prefix) - 1U, &buffered) &&
              profiler_uart_append_uint32(event_count, &buffered) &&
              profiler_uart_append_line(frequency_field, sizeof(frequency_field) - 1U, &buffered) &&
              profiler_uart_append_uint32(profiler_frequency(), &buffered) &&
              profiler_uart_append_line(overflow_field, sizeof(overflow_field) - 1U, &buffered) &&
              profiler_uart_append_uint32(overflowed, &buffered) &&
              profiler_uart_append_line(csv_header, sizeof(csv_header) - 1U, &buffered);

    for (event_index = 0U; (event_index < event_count) && success; event_index++) {
        if (!profiler_read_event(event_index, &event)) {
            success = false;
            break;
        }

        success = profiler_uart_append_uint32(event_index, &buffered) &&
                  profiler_uart_append_char(',', &buffered) &&
                  profiler_uart_append_uint32(event.timestamp, &buffered) &&
                  profiler_uart_append_char(',', &buffered);
        if (success) {
            if (event.event == PROFILER_EVENT_ENTER) {
                success = profiler_uart_append_line("ENTER", 5U, &buffered);
            } else if (event.event == PROFILER_EVENT_EXIT) {
                success = profiler_uart_append_line("EXIT", 4U, &buffered);
            } else {
                success = profiler_uart_append_line("UNKNOWN", 7U, &buffered);
            }
        }

        if (success) {
            success = profiler_uart_append_char(',', &buffered) &&
                      profiler_uart_append_address((uintptr_t)event.this, &buffered) &&
                      profiler_uart_append_char(',', &buffered) &&
                      profiler_uart_append_address((uintptr_t)event.call, &buffered) &&
                      profiler_uart_append_line("\r\n", 2U, &buffered);
        }
    }

    if (success) {
        success = profiler_uart_append_line("# END\r\n", 7U, &buffered);
    }

    if (success) {
        success = profiler_uart_send_chunk((uint16_t)buffered);
    }

    critical_state = sertos_port_enter_critical();
    profiler_start();
    sertos_port_exit_critical(critical_state);

    return success;
}

void sertos_task_profiler_init(void)
{
    profiler_config_t profiler_config;

    profiler_config.frequency = SERTOS_CONFIG_TICK_RATE_HZ;
    profiler_config.buffer = s_profiler_events;
    profiler_config.capacity = PROFILER_EVENT_CAPACITY;
    profiler_init(&profiler_config);
    profiler_start();
}

void sertos_task_profiler(void* param)
{
    (void)param;

    while (1) {
        bool dump_requested;
        uint32_t critical_state;

        if (s_profiler_uart_error) {
            Error_Handler();
        }

        critical_state = sertos_port_enter_critical();
        dump_requested = s_profiler_dump_requested;
        s_profiler_dump_requested = false;
        sertos_port_exit_critical(critical_state);

        if (dump_requested && !profiler_uart_dump_events()) {
            Error_Handler();
        }

        (void)sertos_scheduler_delay(10U);
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* uart, uint16_t size)
{
    HAL_UART_RxEventTypeTypeDef event_type;

    if (uart != &huart2) {
        return;
    }

    event_type = HAL_UARTEx_GetRxEventType(uart);
    if ((event_type == HAL_UART_RXEVENT_IDLE) || (event_type == HAL_UART_RXEVENT_TC)) {
        if (profiler_command_matches(s_profiler_command_buffer, size)) {
            s_profiler_dump_requested = true;
        }

        if (!sertos_task_profiler_start_uart_receive()) {
            s_profiler_uart_error = true;
        }
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* uart)
{
    if (uart == &huart2) {
        s_profiler_tx_complete = true;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* uart)
{
    if (uart == &huart2) {
        s_profiler_tx_error = true;
        s_profiler_uart_error = true;
    }
}
