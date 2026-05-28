#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <time.h>
#include <math.h>
#include <task.h>
#include <stdlib.h>

#include "hardware/uart.h"
#include "configuration.h"
#include "scale.h"
#include "app.h"

/* 
  Example data from Steinberg SBS-LW-300-MAXT:
    W:+     0.00GN  
    W:+     1.14GN  
    W:-   143.02GN  
*/

// Forward declaration
void _steinberg_scale_listener_task(void *p);
extern scale_config_t scale_config;
static void force_zero();

// Instance of the scale handle for Steinberg SBS
scale_handle_t steinberg_scale_handle = {
    .read_loop_task = _steinberg_scale_listener_task,
    .force_zero = force_zero,
};

void _steinberg_scale_listener_task(void *p) {
    char line_buf[64];
    uint8_t line_idx = 0;

    while (true) {
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);

            if (ch == '\n') {
                line_buf[line_idx] = '\0';
                line_idx = 0;

                // Format: "W:+     0.00GN  " or "W:-   143.02GN  "
                if (line_buf[0] == 'W' && line_buf[1] == ':') {
                    char *ptr = line_buf + 2; // Skip "W:"
                    char *endptr;
                    float weight = strtof(ptr, &endptr);

                    if (endptr != ptr) {
                        scale_config.current_scale_measurement = weight;
                        if (scale_config.scale_measurement_ready) {
                            xSemaphoreGive(scale_config.scale_measurement_ready);
                        }
                    }
                }
            } else if (ch != '\r') {
                if (line_idx < sizeof(line_buf) - 1) {
                    line_buf[line_idx++] = ch;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void force_zero() {
    uart_puts(SCALE_UART, "T\r\n");
}
