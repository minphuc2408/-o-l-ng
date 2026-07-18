#ifndef SERIAL_TX_H
#define SERIAL_TX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo module truyền Serial (UART) ở tốc độ baud cấu hình.
 */
void serial_tx_init(void);

/**
 * @brief Đóng gói và gửi một khung dữ liệu âm thanh qua Serial.
 * @param samples Con trỏ tới buffer chứa các mẫu âm thanh cần gửi.
 * @param count Số lượng mẫu cần gửi (thường bằng 32).
 */
void serial_tx_send_frame(const int16_t *samples, uint8_t count);

/**
 * @brief Gửi 32 cặp mẫu CIC Primary/Reference trong phiên calibration thực.
 * Frame: AA 56 SEQ COUNT [Primary:int32 LE, Reference:int32 LE]... XOR.
 */
void serial_tx_send_calibration_frame(const int32_t *primary, const int32_t *reference, uint8_t count);

/**
 * @brief Đọc lệnh host A5 5A 01 <duration_ms:uint32 LE> <xor>.
 * @return true khi đã nhận một lệnh START_CALIBRATION hợp lệ.
 */
bool serial_tx_poll_calibration_start(uint32_t *duration_ms);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_TX_H
