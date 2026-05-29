#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <time.h>
#include <math.h>
#include <task.h>
#include <string.h>
#include <ctype.h>

#include "hardware/uart.h"
#include "configuration.h"
#include "scale.h"
#include "app.h"

/* 
  Steinberg SBS-LW-300-MAXT output format (FT mode, GN unit):
    W:+     0.00GN  \r\n   (18 bytes total)

  Verified via CoolTerm hex capture:
  57 3A 2B 20 20 20 20 20 30 2E 30 30 47 4E 20 20 0D 0A
  W  :  +  sp sp sp sp sp 0  .  0  0  G  N  sp sp CR LF
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
    char line_buf[32];
    uint8_t line_idx = 0;

    while (true) {
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);

            // Prevent buffer overflow
            if (line_idx >= sizeof(line_buf) - 1) {
                line_idx = 0;
            }

            line_buf[line_idx++] = ch;

            if (ch == '\n') {
                line_buf[line_idx] = '\0';
                line_idx = 0;

                // Validate header "W:"
                if (line_buf[0] == 'W' && line_buf[1] == ':') {

                    // Skip "W:", then find first digit or sign
                    // (identical approach to generic_scale driver)
                    char *startptr = line_buf + 2;
                    while (*startptr &&
                           !isdigit((unsigned char)*startptr) &&
                           *startptr != '-' &&
                           *startptr != '+') {
                        startptr++;
                    }

                    char *endptr;
                    float weight = strtof(startptr, &endptr);

                    if (endptr != startptr) {
                        scale_config.current_scale_measurement = weight;
                        if (scale_config.scale_measurement_ready) {
                            xSemaphoreGive(scale_config.scale_measurement_ready);
                        }
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void force_zero() {
    // Tare command - needs testing with Steinberg SBS-LW-300-MAXT
    // Try "Z\r\n" first, fallback options: "T\r\n"
    char cmd[] = "Z\r\n";
    scale_write(cmd, strlen(cmd));
}
