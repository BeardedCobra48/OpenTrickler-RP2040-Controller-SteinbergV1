#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <time.h>
#include <math.h>
#include <task.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "hardware/uart.h"

#include "configuration.h"
#include "scale.h"
#include "app.h"

/*
  Steinberg SBS-LW-300-MAXT output format:

    W:+     0.00GN\r\n

  Example:
    W:+   303.44GN
*/

void _steinberg_scale_listener_task(void *p);

extern scale_config_t scale_config;

static void force_zero();

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

            // Prevent buffer overflow
            if (line_idx >= sizeof(line_buf) - 1) {
                line_idx = 0;
            }

            line_buf[line_idx++] = ch;

            // End of line
            if (ch == '\n') {

                line_buf[line_idx] = '\0';
                line_idx = 0;

                printf("RAW: %s", line_buf);

                // Validate packet
                if (line_buf[0] == 'W' && line_buf[1] == ':') {

                    float weight = 0.0f;

                    // Parse:
                    // W:+     303.44GN
                    if (sscanf(line_buf, "W:+ %fGN", &weight) == 1) {

                        printf("PARSED: %.2f\n", weight);

                        // Store weight
                        scale_config.current_scale_measurement = weight;

                        // Notify waiting tasks
                        if (scale_config.scale_measurement_ready) {
                            xSemaphoreGive(scale_config.scale_measurement_ready);
                        }

                    } else {

                        printf("PARSE FAILED\n");
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void force_zero() {

    // Tare command
    char cmd[] = "Z\r\n";

    scale_write(cmd, strlen(cmd));
}
