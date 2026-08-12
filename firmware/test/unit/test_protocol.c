#include "test_framework.h"
#include "../../src/protocol.h"

#include <string.h>
#include <stdio.h>

void test_protocol_telemetry_roundtrip(void) {
    printf("test_protocol_telemetry_roundtrip:\n");

    telemetry_frame_t original = {
        .cycle_id = 123456u,
        .timestamp_ms = 987654u,
        .pedal_raw_a = 2048u,
        .pedal_raw_b = 2050u,
        .throttle_raw_a = 1024u,
        .throttle_raw_b = 1023u,
        .pedal_pct = 50.5f,
        .throttle_pct = 49.9f,
        .control_error = 0.6f,
        .actuator_duty_pct = -12.25f,
        .active_dtc_mask = DTC_PEDAL_PLAUSIBILITY | DTC_COMMS_TIMEOUT,
        .failsafe_active = 1u,
    };

    uint8_t buf[PROTOCOL_MAX_FRAME_LEN];
    size_t n = protocol_encode_telemetry(&original, buf);
    HELM_CHECK(n == 2 + 1 + 1 + TELEMETRY_PAYLOAD_LEN + 2, "encoded telemetry frame length");

    protocol_rx_t rx;
    protocol_rx_init(&rx);
    uint8_t got_type = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t r = protocol_rx_feed_byte(&rx, buf[i]);
        if (r != 0) {
            got_type = r;
        }
    }
    HELM_CHECK_INT_EQ(got_type, FRAME_TYPE_TELEMETRY, "decoded frame type");
    HELM_CHECK_INT_EQ(rx.crc_error_count, 0, "no CRC errors on valid frame");

    telemetry_frame_t decoded;
    int ok = protocol_decode_telemetry(&rx, &decoded);
    HELM_CHECK_INT_EQ(ok, 1, "telemetry payload length matches");
    HELM_CHECK_INT_EQ(decoded.cycle_id, original.cycle_id, "cycle_id roundtrip");
    HELM_CHECK_INT_EQ(decoded.timestamp_ms, original.timestamp_ms, "timestamp_ms roundtrip");
    HELM_CHECK_INT_EQ(decoded.pedal_raw_a, original.pedal_raw_a, "pedal_raw_a roundtrip");
    HELM_CHECK_INT_EQ(decoded.active_dtc_mask, original.active_dtc_mask, "active_dtc_mask roundtrip");
    HELM_CHECK_INT_EQ(decoded.failsafe_active, original.failsafe_active, "failsafe_active roundtrip");
    HELM_CHECK_FLOAT_LE(decoded.pedal_pct - original.pedal_pct, 0.0001f, "pedal_pct roundtrip (upper)");
    HELM_CHECK_FLOAT_LE(original.pedal_pct - decoded.pedal_pct, 0.0001f, "pedal_pct roundtrip (lower)");
    HELM_CHECK_FLOAT_LE(decoded.actuator_duty_pct - original.actuator_duty_pct, 0.0001f, "actuator_duty_pct roundtrip (upper)");
    HELM_CHECK_FLOAT_LE(original.actuator_duty_pct - decoded.actuator_duty_pct, 0.0001f, "actuator_duty_pct roundtrip (lower)");
}

void test_protocol_command_roundtrip(void) {
    printf("test_protocol_command_roundtrip:\n");

    command_frame_t original = {
        .cmd = CMD_INJECT_FAULT,
        .fault_type = FAULT_TYPE_SENSOR_MISMATCH,
        .target_sensor = TARGET_SENSOR_THROTTLE_B,
        .value_f32 = 42.5f,
    };

    uint8_t buf[PROTOCOL_MAX_FRAME_LEN];
    size_t n = protocol_encode_command(&original, buf);

    protocol_rx_t rx;
    protocol_rx_init(&rx);
    uint8_t got_type = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t r = protocol_rx_feed_byte(&rx, buf[i]);
        if (r != 0) got_type = r;
    }
    HELM_CHECK_INT_EQ(got_type, FRAME_TYPE_COMMAND, "decoded frame type");

    command_frame_t decoded;
    int ok = protocol_decode_command(&rx, &decoded);
    HELM_CHECK_INT_EQ(ok, 1, "command payload length matches");
    HELM_CHECK_INT_EQ(decoded.cmd, original.cmd, "cmd roundtrip");
    HELM_CHECK_INT_EQ(decoded.fault_type, original.fault_type, "fault_type roundtrip");
    HELM_CHECK_INT_EQ(decoded.target_sensor, original.target_sensor, "target_sensor roundtrip");
    HELM_CHECK_FLOAT_LE(decoded.value_f32 - original.value_f32, 0.0001f, "value_f32 roundtrip (upper)");
    HELM_CHECK_FLOAT_LE(original.value_f32 - decoded.value_f32, 0.0001f, "value_f32 roundtrip (lower)");
}

void test_protocol_crc_mismatch_detected_and_resyncs(void) {
    printf("test_protocol_crc_mismatch_detected_and_resyncs:\n");

    ack_frame_t frame = { .cycle_id = 7u, .cmd_echo = CMD_PING, .status = ACK_STATUS_OK };
    uint8_t buf[PROTOCOL_MAX_FRAME_LEN];
    size_t n = protocol_encode_ack(&frame, buf);

    /* Corrupt one payload byte so the CRC no longer matches. */
    buf[5] ^= 0xFFu;

    protocol_rx_t rx;
    protocol_rx_init(&rx);
    uint8_t got_type = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t r = protocol_rx_feed_byte(&rx, buf[i]);
        if (r != 0) got_type = r;
    }
    HELM_CHECK_INT_EQ(got_type, 0, "corrupted frame must not be reported as valid");
    HELM_CHECK_INT_EQ(rx.crc_error_count, 1, "corrupted frame must increment crc_error_count exactly once");

    /* Receiver must resync: a valid frame sent right after must still decode. */
    uint8_t buf2[PROTOCOL_MAX_FRAME_LEN];
    size_t n2 = protocol_encode_ack(&frame, buf2);
    got_type = 0;
    for (size_t i = 0; i < n2; i++) {
        uint8_t r = protocol_rx_feed_byte(&rx, buf2[i]);
        if (r != 0) got_type = r;
    }
    HELM_CHECK_INT_EQ(got_type, FRAME_TYPE_ACK, "receiver resyncs and decodes the next valid frame");
}

void test_protocol_crc16_known_vector(void) {
    printf("test_protocol_crc16_known_vector:\n");
    /* CRC-16/CCITT-FALSE("123456789") == 0x29B1, a standard published check value. */
    const uint8_t check[] = "123456789";
    uint16_t crc = protocol_crc16_ccitt_false(check, 9);
    HELM_CHECK_INT_EQ(crc, 0x29B1, "CRC-16/CCITT-FALSE check value for \"123456789\"");
}
