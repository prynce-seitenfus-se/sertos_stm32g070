#include "sertos_task_profiler.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "main.h"
#include "profiler.h"
#include "sertos_port.h"
#include "sertos_scheduler.h"
#include "stream.h"
#include "usart.h"
#include "stm32g0xx_hal_uart_ex.h"

#define PROFILER_EVENT_CAPACITY       (1024U)
#define PROFILER_COMMAND_BUFFER_SIZE  (32U)
#define PROFILER_TX_BUFFER_SIZE       (256U)
#define PROFILER_TX_TIMEOUT_MS        (1000U)
#define PROFILER_DUMP_COMMAND         "prof-dump"
#define PROFILER_DUMP_COMMAND_LENGTH  (sizeof(PROFILER_DUMP_COMMAND) - 1U)

/* Forward declarations of internal static functions */
static bool profiler_command_matches(const uint8_t* buffer, uint16_t size);
static bool profiler_uart_send_chunk(uint16_t length);
static size_t profiler_uart_stream_write(void* context, const uint8_t* buffer, size_t size);
static void profiler_uart_stream_flush(void* context);
static size_t profiler_uart_stream_read(void* context, uint8_t* buffer, size_t size);
static bool profiler_stream_append_line(const Stream* stream, const char* line, size_t length);
static bool profiler_stream_append_char(const Stream* stream, char value);
static bool profiler_stream_append_uint32(const Stream* stream, uint32_t value);
static bool profiler_stream_append_address(const Stream* stream, uintptr_t address);
static bool profiler_uart_dump_events(const Stream* stream);

/* Static members and state variables */
static profiler_event_t s_profiler_events[PROFILER_EVENT_CAPACITY];
static uint8_t s_profiler_command_buffer[PROFILER_COMMAND_BUFFER_SIZE]
    __attribute__((aligned(32)));
static uint8_t s_profiler_tx_buffer[PROFILER_TX_BUFFER_SIZE]
    __attribute__((aligned(32)));

static Stream s_profiler_stream;
static size_t s_profiler_tx_buffered = 0U;
static volatile size_t s_profiler_rx_available = 0U;
static size_t s_profiler_rx_read_offset = 0U;

static volatile bool s_profiler_dump_requested = false;
static volatile bool s_profiler_tx_complete = false;
static volatile bool s_profiler_tx_error = false;
static volatile bool s_profiler_uart_error = false;

/* ========================================================================== */
/* Public API Implementations                                                 */
/* ========================================================================== */

void sertos_task_profiler_init(void)
{
    profiler_config_t profiler_config;

    /* Initialize bidirectional stream connected to USART2 */
    s_profiler_stream = stream_init(&huart2,
                                    profiler_uart_stream_write,
                                    profiler_uart_stream_read,
                                    profiler_uart_stream_flush);

    /* TIM1 drives the profiler timestamp counter (prescaler 0), so its tick
     * rate equals the APB1 timer clock. With APB1 prescaler = 1 the timer
     * clock equals PCLK1, which is the correct frequency for dump analysis. */
    profiler_config.frequency = HAL_RCC_GetPCLK1Freq();
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

        if (dump_requested && !profiler_uart_dump_events(&s_profiler_stream)) {
            Error_Handler();
        }

        (void)sertos_scheduler_delay(10U);
    }
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

/* ========================================================================== */
/* Static Helper Functions                                                    */
/* ========================================================================== */

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

static size_t profiler_uart_stream_write(void* context, const uint8_t* buffer, size_t size)
{
    UART_HandleTypeDef* uart = (UART_HandleTypeDef*)context;
    size_t written = 0U;

    if ((uart == NULL) || (buffer == NULL) || (size == 0U)) {
        return 0U;
    }

    while (written < size) {
        size_t available = sizeof(s_profiler_tx_buffer) - s_profiler_tx_buffered;
        size_t remaining = size - written;
        size_t to_copy = (remaining < available) ? remaining : available;

        (void)memcpy(&s_profiler_tx_buffer[s_profiler_tx_buffered], &buffer[written], to_copy);
        s_profiler_tx_buffered += to_copy;
        written += to_copy;

        if (s_profiler_tx_buffered >= sizeof(s_profiler_tx_buffer)) {
            if (!profiler_uart_send_chunk((uint16_t)s_profiler_tx_buffered)) {
                break;
            }
            s_profiler_tx_buffered = 0U;
        }
    }

    return written;
}

static void profiler_uart_stream_flush(void* context)
{
    (void)context;
    if (s_profiler_tx_buffered > 0U) {
        (void)profiler_uart_send_chunk((uint16_t)s_profiler_tx_buffered);
        s_profiler_tx_buffered = 0U;
    }
}

static size_t profiler_uart_stream_read(void* context, uint8_t* buffer, size_t size)
{
    (void)context;
    size_t read_bytes = 0U;

    if ((buffer == NULL) || (size == 0U) || (s_profiler_rx_available == 0U)) {
        return 0U;
    }

    size_t remaining = s_profiler_rx_available - s_profiler_rx_read_offset;
    read_bytes = (size < remaining) ? size : remaining;

    (void)memcpy(buffer, &s_profiler_command_buffer[s_profiler_rx_read_offset], read_bytes);
    s_profiler_rx_read_offset += read_bytes;

    if (s_profiler_rx_read_offset >= s_profiler_rx_available) {
        s_profiler_rx_available = 0U;
        s_profiler_rx_read_offset = 0U;
    }

    return read_bytes;
}

static bool profiler_stream_append_line(const Stream* stream, const char* line, size_t length)
{
    if ((stream == NULL) || (line == NULL)) {
        return false;
    }
    return (stream_write(stream, (const uint8_t*)line, length) == length);
}

static bool profiler_stream_append_char(const Stream* stream, char value)
{
    return profiler_stream_append_line(stream, &value, 1U);
}

static bool profiler_stream_append_uint32(const Stream* stream, uint32_t value)
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
        if (!profiler_stream_append_char(stream, digits[count])) {
            return false;
        }
    }

    return true;
}

static bool profiler_stream_append_address(const Stream* stream, uintptr_t address)
{
    static const char hexadecimal_digits[] = "0123456789abcdef";
    size_t digits = sizeof(uintptr_t) * 2U;

    if (!profiler_stream_append_line(stream, "0x", 2U)) {
        return false;
    }

    while (digits > 0U) {
        uint8_t digit;

        digits--;
        digit = (uint8_t)((address >> (digits * 4U)) & 0x0FU);
        if (!profiler_stream_append_char(stream, hexadecimal_digits[digit])) {
            return false;
        }
    }

    return true;
}

static bool profiler_uart_dump_events(const Stream* stream)
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
    bool success;

    if (stream == NULL) {
        return false;
    }

    profiler_stop();
    overflowed = profiler_overflowed() ? 1U : 0U;
    capacity = profiler_capacity();
    while ((event_count < capacity) && profiler_read_event(event_count, &event)) {
        event_count++;
    }

    success = profiler_stream_append_line(stream, dump_prefix, sizeof(dump_prefix) - 1U) &&
              profiler_stream_append_uint32(stream, event_count) &&
              profiler_stream_append_line(stream, frequency_field, sizeof(frequency_field) - 1U) &&
              profiler_stream_append_uint32(stream, profiler_frequency()) &&
              profiler_stream_append_line(stream, overflow_field, sizeof(overflow_field) - 1U) &&
              profiler_stream_append_uint32(stream, overflowed) &&
              profiler_stream_append_line(stream, csv_header, sizeof(csv_header) - 1U);

    for (event_index = 0U; (event_index < event_count) && success; event_index++) {
        if (!profiler_read_event(event_index, &event)) {
            success = false;
            break;
        }

        success = profiler_stream_append_uint32(stream, event_index) &&
                  profiler_stream_append_char(stream, ',') &&
                  profiler_stream_append_uint32(stream, event.timestamp) &&
                  profiler_stream_append_char(stream, ',');
        if (success) {
            if (event.event == PROFILER_EVENT_ENTER) {
                success = profiler_stream_append_line(stream, "ENTER", 5U);
            } else if (event.event == PROFILER_EVENT_EXIT) {
                success = profiler_stream_append_line(stream, "EXIT", 4U);
            } else {
                success = profiler_stream_append_line(stream, "UNKNOWN", 7U);
            }
        }

        if (success) {
            success = profiler_stream_append_char(stream, ',') &&
                      profiler_stream_append_address(stream, (uintptr_t)event.this) &&
                      profiler_stream_append_char(stream, ',') &&
                      profiler_stream_append_address(stream, (uintptr_t)event.call) &&
                      profiler_stream_append_line(stream, "\r\n", 2U);
        }
    }

    if (success) {
        success = profiler_stream_append_line(stream, "# END\r\n", 7U);
    }

    stream_flush(stream);

    critical_state = sertos_port_enter_critical();
    profiler_start();
    sertos_port_exit_critical(critical_state);

    return success;
}

/* ========================================================================== */
/* HAL Callbacks                                                              */
/* ========================================================================== */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* uart, uint16_t size)
{
    HAL_UART_RxEventTypeTypeDef event_type;

    if (uart != &huart2) {
        return;
    }

    event_type = HAL_UARTEx_GetRxEventType(uart);
    if ((event_type == HAL_UART_RXEVENT_IDLE) || (event_type == HAL_UART_RXEVENT_TC)) {
        uint8_t cmd_buffer[PROFILER_COMMAND_BUFFER_SIZE];
        s_profiler_rx_available = (size <= sizeof(s_profiler_command_buffer)) ?
                                  (size_t)size : sizeof(s_profiler_command_buffer);
        s_profiler_rx_read_offset = 0U;

        size_t bytes_read = stream_read(&s_profiler_stream, cmd_buffer, sizeof(cmd_buffer));
        if (profiler_command_matches(cmd_buffer, (uint16_t)bytes_read)) {
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
