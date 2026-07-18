#include "serial_tx.h"
#include "config.h"
#include <Arduino.h>

static uint8_t calibration_seq_num = 0;

#define CALIBRATION_SYNC_0 0xAA
#define CALIBRATION_SYNC_1 0x56
#define CONTROL_SYNC_0 0xA5
#define CONTROL_SYNC_1 0x5A
#define CONTROL_START_CALIBRATION 0x01

void serial_tx_init(void) {
    // BẮT BUỘC: Gọi setTxBufferSize trước khi begin để ESP32 Arduino thiết lập bộ đệm ring buffer TX mới
    Serial.setTxBufferSize(1024);
    Serial.begin(UART_BAUD_RATE);
}

void serial_tx_send_calibration_frame(const int32_t *primary, const int32_t *reference, uint8_t count) {
    if (primary == NULL || reference == NULL || count == 0 || count > 32) {
        return;
    }

    const uint16_t frame_size = 2U + 1U + 1U + ((uint16_t)count * 8U) + 1U;
    uint8_t frame_buf[261];
    if (frame_size > sizeof(frame_buf)) {
        return;
    }

    frame_buf[0] = CALIBRATION_SYNC_0;
    frame_buf[1] = CALIBRATION_SYNC_1;
    frame_buf[2] = calibration_seq_num;
    frame_buf[3] = count;

    for (uint8_t i = 0; i < count; i++) {
        const uint32_t p = (uint32_t)primary[i];
        const uint32_t r = (uint32_t)reference[i];
        const uint16_t offset = 4U + ((uint16_t)i * 8U);
        frame_buf[offset] = (uint8_t)(p & 0xFFU);
        frame_buf[offset + 1U] = (uint8_t)((p >> 8U) & 0xFFU);
        frame_buf[offset + 2U] = (uint8_t)((p >> 16U) & 0xFFU);
        frame_buf[offset + 3U] = (uint8_t)((p >> 24U) & 0xFFU);
        frame_buf[offset + 4U] = (uint8_t)(r & 0xFFU);
        frame_buf[offset + 5U] = (uint8_t)((r >> 8U) & 0xFFU);
        frame_buf[offset + 6U] = (uint8_t)((r >> 16U) & 0xFFU);
        frame_buf[offset + 7U] = (uint8_t)((r >> 24U) & 0xFFU);
    }

    uint8_t checksum = 0;
    for (uint16_t i = 0; i < frame_size - 1U; i++) {
        checksum ^= frame_buf[i];
    }
    frame_buf[frame_size - 1U] = checksum;
    Serial.write(frame_buf, frame_size);
    calibration_seq_num++;
}

bool serial_tx_poll_calibration_start(uint32_t *duration_ms) {
    static uint8_t state = 0;
    static uint8_t payload[5];
    static uint8_t payload_index = 0;

    while (Serial.available() > 0) {
        const uint8_t byte_in = (uint8_t)Serial.read();
        switch (state) {
            case 0:
                state = (byte_in == CONTROL_SYNC_0) ? 1 : 0;
                break;
            case 1:
                state = (byte_in == CONTROL_SYNC_1) ? 2 : 0;
                break;
            case 2:
                if (byte_in == CONTROL_START_CALIBRATION) {
                    payload_index = 0;
                    state = 3;
                } else {
                    state = 0;
                }
                break;
            case 3:
                payload[payload_index++] = byte_in;
                if (payload_index == sizeof(payload)) {
                    uint8_t checksum = CONTROL_SYNC_0 ^ CONTROL_SYNC_1 ^ CONTROL_START_CALIBRATION;
                    for (uint8_t i = 0; i < 4; i++) checksum ^= payload[i];
                    state = 0;
                    if (checksum == payload[4]) {
                        *duration_ms = ((uint32_t)payload[0]) |
                                       ((uint32_t)payload[1] << 8U) |
                                       ((uint32_t)payload[2] << 16U) |
                                       ((uint32_t)payload[3] << 24U);
                        return true;
                    }
                }
                break;
        }
    }
    return false;
}
