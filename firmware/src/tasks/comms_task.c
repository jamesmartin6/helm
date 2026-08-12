#include "comms_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "../config.h"
#include "../shared_state.h"
#include "../protocol.h"
#include "../uart.h"

static void handle_command(const command_frame_t *cmd) {
    ack_frame_t ack;
    ack.cycle_id = g_current_cycle;
    ack.cmd_echo = cmd->cmd;
    ack.status = ACK_STATUS_OK;

    switch (cmd->cmd) {
    case CMD_SET_PEDAL_OVERRIDE:
        if (cmd->value_f32 < -100.0f || cmd->value_f32 > 100.0f) {
            ack.status = ACK_STATUS_REJECTED;
        } else {
            g_pedal_override_pct = cmd->value_f32;
        }
        break;

    case CMD_INJECT_FAULT:
        if (cmd->target_sensor >= NUM_SENSOR_CHANNELS) {
            ack.status = ACK_STATUS_REJECTED;
            break;
        }
        switch (cmd->fault_type) {
        case FAULT_TYPE_SENSOR_STUCK:
            g_fault_overrides[cmd->target_sensor].kind = FAULT_OVERRIDE_STUCK;
            g_fault_overrides[cmd->target_sensor].value_pct = cmd->value_f32;
            break;
        case FAULT_TYPE_SENSOR_OUT_OF_RANGE:
            g_fault_overrides[cmd->target_sensor].kind = FAULT_OVERRIDE_OUT_OF_RANGE;
            break;
        case FAULT_TYPE_SENSOR_MISMATCH:
            g_fault_overrides[cmd->target_sensor].kind = FAULT_OVERRIDE_MISMATCH;
            break;
        case FAULT_TYPE_COMMS_DROPOUT:
            /* No firmware-side action: a real comms dropout is simulated by
             * the harness simply going silent, which diag_task already
             * detects via COMMS_TIMEOUT_CYCLES. Ack'd as a no-op. */
            break;
        default:
            ack.status = ACK_STATUS_REJECTED;
            break;
        }
        break;

    case CMD_CLEAR_FAULT:
        if (cmd->target_sensor >= NUM_SENSOR_CHANNELS) {
            ack.status = ACK_STATUS_REJECTED;
        } else {
            g_fault_overrides[cmd->target_sensor].kind = FAULT_OVERRIDE_NONE;
        }
        break;

    case CMD_PING:
        break;

    default:
        ack.status = ACK_STATUS_REJECTED;
        break;
    }

    uint8_t buf[PROTOCOL_MAX_FRAME_LEN];
    size_t n = protocol_encode_ack(&ack, buf);
    for (size_t i = 0; i < n; i++) {
        uart_putc(buf[i]);
    }
}

void comms_task(void *pvParameters) {
    (void)pvParameters;

    uart_init();

    protocol_rx_t rx;
    protocol_rx_init(&rx);

    telemetry_frame_t last_sent_frame;
    int have_frame = 0;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&last_wake, period);

        /* TX: send the latest telemetry frame, one per control cycle. */
        telemetry_frame_t frame;
        if (xQueueReceive(g_control_to_comms_queue, &frame, 0) == pdTRUE) {
            last_sent_frame = frame;
            have_frame = 1;
        }
        if (have_frame) {
            uint8_t buf[PROTOCOL_MAX_FRAME_LEN];
            size_t n = protocol_encode_telemetry(&last_sent_frame, buf);
            for (size_t i = 0; i < n; i++) {
                uart_putc(buf[i]);
            }
        }

        /* RX: drain whatever inbound bytes are waiting; decode/dispatch any
         * complete, CRC-valid command frames found. */
        uint8_t byte;
        while (uart_try_getc(&byte)) {
            uint8_t frame_type = protocol_rx_feed_byte(&rx, byte);
            if (frame_type == FRAME_TYPE_COMMAND) {
                command_frame_t cmd;
                if (protocol_decode_command(&rx, &cmd)) {
                    g_last_valid_cmd_cycle = g_current_cycle;
                    handle_command(&cmd);
                }
            }
        }
    }
}
