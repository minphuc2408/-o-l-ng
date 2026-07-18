#ifndef HEART_RATE_H
#define HEART_RATE_H

#include <stdint.h>

/**
 * @brief Xóa toàn bộ trạng thái của bộ phát hiện nhịp tim.
 */
void heart_rate_reset(void);

/**
 * @brief Xử lý liên tục một mẫu e(n) 8 kHz sau ANC.
 *
 * Hàm không cấp phát động. Khi phát hiện peak, hàm ghi:
 * "Peak detected | interval=XXXms | amplitude=YYY | BPM=XX"
 *
 * @param e_n Mẫu tín hiệu tim đã khử nhiễu và scale về int16_t.
 */
void processSample(int16_t e_n);

/**
 * @brief Trả về BPM gần nhất, hoặc 0 nếu chưa đủ 8 interval hợp lệ.
 *
 * Detector được giới hạn cho nhịp nghỉ: interval hợp lệ 600–1500 ms
 * (xấp xỉ 40–100 BPM), có khóa nhịp median để loại candidate S2.
 */
uint16_t heart_rate_get_bpm(void);

#endif // HEART_RATE_H
