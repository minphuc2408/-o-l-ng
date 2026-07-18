#ifndef I2S_DRIVER_H
#define I2S_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo driver I2S cho cấu hình stereo 2 microphone.
 * @return esp_err_t ESP_OK nếu thành công, hoặc mã lỗi ESP-IDF.
 */
esp_err_t i2s_driver_init(void);

/**
 * @brief Đọc dữ liệu từ 2 mic và tách thành 2 buffer riêng biệt (Primary và Reference).
 * @param primary_buf Con trỏ đến buffer chứa kênh Left (Mic 1 - Primary).
 * @param reference_buf Con trỏ đến buffer chứa kênh Right (Mic 2 - Reference).
 * @param max_pairs Số lượng cặp mẫu tối đa cần đọc (bằng SAMPLE_BUFFER_SIZE).
 * @return size_t Số lượng cặp mẫu thực tế đọc và tách thành công.
 */
size_t i2s_driver_read(int32_t *primary_buf, int32_t *reference_buf, size_t max_pairs);

/**
 * @brief Lấy số lượng mẫu bị rớt (DMA overflow) đã đếm được.
 * @return uint32_t Số lượng mẫu rớt.
 */
uint32_t i2s_driver_get_overflow_count(void);

#ifdef __cplusplus
}
#endif

#endif // I2S_DRIVER_H
