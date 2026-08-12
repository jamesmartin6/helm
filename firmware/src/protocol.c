#include "protocol.h"

uint16_t protocol_crc16_ccitt_false(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static uint8_t *put_u8(uint8_t *p, uint8_t v) {
    *p = v;
    return p + 1;
}

static uint8_t *put_u16le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    return p + 2;
}

static uint8_t *put_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
    return p + 4;
}

static uint8_t *put_f32le(uint8_t *p, float v) {
    uint32_t bits;
    /* memcpy-free bit reinterpret via union; avoids strict-aliasing UB. */
    union { float f; uint32_t u; } conv;
    conv.f = v;
    bits = conv.u;
    return put_u32le(p, bits);
}

static const uint8_t *get_u16le(const uint8_t *p, uint16_t *out) {
    *out = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
    return p + 2;
}

static const uint8_t *get_u32le(const uint8_t *p, uint32_t *out) {
    *out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
    return p + 4;
}

static const uint8_t *get_f32le(const uint8_t *p, float *out) {
    uint32_t bits;
    union { float f; uint32_t u; } conv;
    p = get_u32le(p, &bits);
    conv.u = bits;
    *out = conv.f;
    return p;
}

/* Builds [SYNC0 SYNC1 LEN TYPE PAYLOAD... CRC16] into out, returns total length. */
static size_t finish_frame(uint8_t *out, uint8_t type, const uint8_t *payload,
                            size_t payload_len) {
    uint8_t *p = out;
    p = put_u8(p, PROTOCOL_SYNC_0);
    p = put_u8(p, PROTOCOL_SYNC_1);
    p = put_u8(p, (uint8_t)(1u + payload_len));
    uint8_t *crc_start = p; /* TYPE + PAYLOAD */
    p = put_u8(p, type);
    for (size_t i = 0; i < payload_len; i++) {
        p = put_u8(p, payload[i]);
    }
    uint16_t crc = protocol_crc16_ccitt_false(crc_start, 1u + payload_len);
    p = put_u16le(p, crc);
    return (size_t)(p - out);
}

size_t protocol_encode_telemetry(const telemetry_frame_t *frame, uint8_t *out) {
    uint8_t payload[TELEMETRY_PAYLOAD_LEN];
    uint8_t *p = payload;
    p = put_u32le(p, frame->cycle_id);
    p = put_u32le(p, frame->timestamp_ms);
    p = put_u16le(p, frame->pedal_raw_a);
    p = put_u16le(p, frame->pedal_raw_b);
    p = put_u16le(p, frame->throttle_raw_a);
    p = put_u16le(p, frame->throttle_raw_b);
    p = put_f32le(p, frame->pedal_pct);
    p = put_f32le(p, frame->throttle_pct);
    p = put_f32le(p, frame->control_error);
    p = put_f32le(p, frame->actuator_duty_pct);
    p = put_u16le(p, frame->active_dtc_mask);
    p = put_u8(p, frame->failsafe_active);
    return finish_frame(out, FRAME_TYPE_TELEMETRY, payload, TELEMETRY_PAYLOAD_LEN);
}

size_t protocol_encode_command(const command_frame_t *frame, uint8_t *out) {
    uint8_t payload[COMMAND_PAYLOAD_LEN];
    uint8_t *p = payload;
    p = put_u8(p, frame->cmd);
    p = put_u8(p, frame->fault_type);
    p = put_u8(p, frame->target_sensor);
    p = put_f32le(p, frame->value_f32);
    return finish_frame(out, FRAME_TYPE_COMMAND, payload, COMMAND_PAYLOAD_LEN);
}

size_t protocol_encode_ack(const ack_frame_t *frame, uint8_t *out) {
    uint8_t payload[ACK_PAYLOAD_LEN];
    uint8_t *p = payload;
    p = put_u32le(p, frame->cycle_id);
    p = put_u8(p, frame->cmd_echo);
    p = put_u8(p, frame->status);
    return finish_frame(out, FRAME_TYPE_ACK, payload, ACK_PAYLOAD_LEN);
}

enum {
    RX_STATE_WAIT_SYNC0 = 0,
    RX_STATE_WAIT_SYNC1,
    RX_STATE_WAIT_LEN,
    RX_STATE_WAIT_TYPE,
    RX_STATE_READ_PAYLOAD,
    RX_STATE_READ_CRC,
};

void protocol_rx_init(protocol_rx_t *rx) {
    rx->state = RX_STATE_WAIT_SYNC0;
    rx->len = 0;
    rx->type = 0;
    rx->payload_idx = 0;
    rx->crc_idx = 0;
    rx->crc_error_count = 0;
}

uint8_t protocol_rx_feed_byte(protocol_rx_t *rx, uint8_t byte) {
    switch (rx->state) {
    case RX_STATE_WAIT_SYNC0:
        if (byte == PROTOCOL_SYNC_0) {
            rx->state = RX_STATE_WAIT_SYNC1;
        }
        return 0;

    case RX_STATE_WAIT_SYNC1:
        if (byte == PROTOCOL_SYNC_1) {
            rx->state = RX_STATE_WAIT_LEN;
        } else if (byte != PROTOCOL_SYNC_0) {
            rx->state = RX_STATE_WAIT_SYNC0;
        }
        return 0;

    case RX_STATE_WAIT_LEN:
        rx->len = byte;
        if (rx->len == 0 || (size_t)(rx->len - 1) > TELEMETRY_PAYLOAD_LEN) {
            rx->state = RX_STATE_WAIT_SYNC0; /* implausible length: resync */
            return 0;
        }
        rx->state = RX_STATE_WAIT_TYPE;
        return 0;

    case RX_STATE_WAIT_TYPE:
        rx->type = byte;
        rx->payload_idx = 0;
        rx->crc_idx = 0;
        rx->state = (rx->len > 1) ? RX_STATE_READ_PAYLOAD : RX_STATE_READ_CRC;
        return 0;

    case RX_STATE_READ_PAYLOAD:
        rx->payload[rx->payload_idx++] = byte;
        if (rx->payload_idx >= (uint8_t)(rx->len - 1)) {
            rx->state = RX_STATE_READ_CRC;
        }
        return 0;

    case RX_STATE_READ_CRC:
        rx->crc_bytes[rx->crc_idx++] = byte;
        if (rx->crc_idx < 2) {
            return 0;
        }
        {
            uint16_t received_crc =
                (uint16_t)(rx->crc_bytes[0] | ((uint16_t)rx->crc_bytes[1] << 8));
            /* Recompute CRC over TYPE + PAYLOAD. */
            uint8_t crc_buf[1 + TELEMETRY_PAYLOAD_LEN];
            crc_buf[0] = rx->type;
            for (uint8_t i = 0; i < rx->payload_idx; i++) {
                crc_buf[1 + i] = rx->payload[i];
            }
            uint16_t computed_crc =
                protocol_crc16_ccitt_false(crc_buf, (size_t)(1 + rx->payload_idx));

            rx->state = RX_STATE_WAIT_SYNC0;
            if (computed_crc != received_crc) {
                rx->crc_error_count++;
                return 0;
            }
            return rx->type;
        }

    default:
        rx->state = RX_STATE_WAIT_SYNC0;
        return 0;
    }
}

int protocol_decode_command(const protocol_rx_t *rx, command_frame_t *out) {
    if ((size_t)(rx->len - 1) != COMMAND_PAYLOAD_LEN) {
        return 0;
    }
    const uint8_t *p = rx->payload;
    out->cmd = *p++;
    out->fault_type = *p++;
    out->target_sensor = *p++;
    p = get_f32le(p, &out->value_f32);
    return 1;
}

int protocol_decode_telemetry(const protocol_rx_t *rx, telemetry_frame_t *out) {
    if ((size_t)(rx->len - 1) != TELEMETRY_PAYLOAD_LEN) {
        return 0;
    }
    const uint8_t *p = rx->payload;
    p = get_u32le(p, &out->cycle_id);
    p = get_u32le(p, &out->timestamp_ms);
    p = get_u16le(p, &out->pedal_raw_a);
    p = get_u16le(p, &out->pedal_raw_b);
    p = get_u16le(p, &out->throttle_raw_a);
    p = get_u16le(p, &out->throttle_raw_b);
    p = get_f32le(p, &out->pedal_pct);
    p = get_f32le(p, &out->throttle_pct);
    p = get_f32le(p, &out->control_error);
    p = get_f32le(p, &out->actuator_duty_pct);
    p = get_u16le(p, &out->active_dtc_mask);
    out->failsafe_active = *p;
    return 1;
}

int protocol_decode_ack(const protocol_rx_t *rx, ack_frame_t *out) {
    if ((size_t)(rx->len - 1) != ACK_PAYLOAD_LEN) {
        return 0;
    }
    const uint8_t *p = rx->payload;
    p = get_u32le(p, &out->cycle_id);
    out->cmd_echo = *p++;
    out->status = *p;
    return 1;
}
