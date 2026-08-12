#ifndef HELM_PROTOCOL_H
#define HELM_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/*
 * Wire protocol shared between firmware and backend/harness.
 * All multi-byte fields little-endian. Every frame:
 *   [0xAA 0x55] [LEN:u8] [TYPE:u8] [PAYLOAD...] [CRC16:u16]
 * LEN = 1 (TYPE byte) + payload length.
 * CRC16 is CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflect, no
 * xorout) computed over TYPE + PAYLOAD.
 */

#define PROTOCOL_SYNC_0 0xAAu
#define PROTOCOL_SYNC_1 0x55u

#define FRAME_TYPE_TELEMETRY 0x01u
#define FRAME_TYPE_COMMAND   0x10u
#define FRAME_TYPE_ACK       0x11u

#define CMD_SET_PEDAL_OVERRIDE 0x01u
#define CMD_INJECT_FAULT       0x02u
#define CMD_CLEAR_FAULT        0x03u
#define CMD_PING                0x04u

#define FAULT_TYPE_NONE            0x00u
#define FAULT_TYPE_SENSOR_STUCK    0x01u
#define FAULT_TYPE_SENSOR_OUT_OF_RANGE 0x02u
#define FAULT_TYPE_SENSOR_MISMATCH 0x03u
#define FAULT_TYPE_COMMS_DROPOUT   0x04u

#define TARGET_SENSOR_PEDAL_A    0x00u
#define TARGET_SENSOR_PEDAL_B    0x01u
#define TARGET_SENSOR_THROTTLE_A 0x02u
#define TARGET_SENSOR_THROTTLE_B 0x03u

#define ACK_STATUS_OK       0x00u
#define ACK_STATUS_REJECTED 0x01u

/* DTC bitmask values (active_dtc_mask in the telemetry frame). */
#define DTC_PEDAL_PLAUSIBILITY     0x0001u
#define DTC_THROTTLE_PLAUSIBILITY  0x0002u
#define DTC_SENSOR_OUT_OF_RANGE    0x0004u
#define DTC_COMMS_TIMEOUT          0x0008u
#define DTC_WATCHDOG_TRIP          0x0010u
#define DTC_ACTUATOR_FAULT         0x0020u

#define TELEMETRY_PAYLOAD_LEN 35u
#define COMMAND_PAYLOAD_LEN    7u
#define ACK_PAYLOAD_LEN        6u

/* Max encoded frame size across all frame types (sync+len+type+payload+crc). */
#define PROTOCOL_MAX_FRAME_LEN (2u + 1u + 1u + TELEMETRY_PAYLOAD_LEN + 2u)

typedef struct {
    uint32_t cycle_id;
    uint32_t timestamp_ms;
    uint16_t pedal_raw_a;
    uint16_t pedal_raw_b;
    uint16_t throttle_raw_a;
    uint16_t throttle_raw_b;
    float pedal_pct;
    float throttle_pct;
    float control_error;
    float actuator_duty_pct;
    uint16_t active_dtc_mask;
    uint8_t failsafe_active;
} telemetry_frame_t;

typedef struct {
    uint8_t cmd;
    uint8_t fault_type;
    uint8_t target_sensor;
    float value_f32;
} command_frame_t;

typedef struct {
    uint32_t cycle_id;
    uint8_t cmd_echo;
    uint8_t status;
} ack_frame_t;

uint16_t protocol_crc16_ccitt_false(const uint8_t *data, size_t len);

/* Encodes a full frame (sync+len+type+payload+crc) into `out`, which must be
 * at least PROTOCOL_MAX_FRAME_LEN bytes. Returns the number of bytes written. */
size_t protocol_encode_telemetry(const telemetry_frame_t *frame, uint8_t *out);
size_t protocol_encode_command(const command_frame_t *frame, uint8_t *out);
size_t protocol_encode_ack(const ack_frame_t *frame, uint8_t *out);

/*
 * Streaming byte-at-a-time frame receiver. Feed bytes as they arrive from
 * UART; when a complete, CRC-valid frame has been assembled, the relevant
 * out-parameter is filled and the function returns the frame's TYPE byte.
 * Returns 0 if no complete frame is available yet after this byte.
 * On CRC mismatch the receiver silently resyncs (scans for the next sync
 * sequence) and returns 0 -- callers that want to count errors can compare
 * `protocol_rx_crc_error_count()` before/after.
 */
typedef struct {
    int state;
    uint8_t len;
    uint8_t type;
    uint8_t payload[TELEMETRY_PAYLOAD_LEN];
    uint8_t payload_idx;
    uint8_t crc_bytes[2];
    uint8_t crc_idx;
    uint32_t crc_error_count;
} protocol_rx_t;

void protocol_rx_init(protocol_rx_t *rx);
uint8_t protocol_rx_feed_byte(protocol_rx_t *rx, uint8_t byte);

/* Decodes a validated payload (rx->payload, rx->len-1 bytes) for the given
 * frame type. Returns 1 on success, 0 if the payload length doesn't match
 * the expected size for that type. */
int protocol_decode_command(const protocol_rx_t *rx, command_frame_t *out);
int protocol_decode_telemetry(const protocol_rx_t *rx, telemetry_frame_t *out);
int protocol_decode_ack(const protocol_rx_t *rx, ack_frame_t *out);

#endif /* HELM_PROTOCOL_H */
