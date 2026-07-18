#ifndef ANC_FILTER_H
#define ANC_FILTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cấu trúc đại diện cho bộ lọc thích nghi ANC NLMS.
 */
typedef struct anc_filter_s anc_filter_t;

/**
 * @brief Khởi tạo một instance bộ lọc thích nghi ANC NLMS.
 * 
 * @param num_taps Số lượng tap lọc (N).
 * @param mu Hệ số bước học khởi tạo (mu).
 * @param epsilon Hệ số epsilon tránh chia cho 0.
 * @return anc_filter_t* Con trỏ tới bộ lọc được cấp phát, hoặc NULL nếu lỗi.
 */
anc_filter_t* anc_filter_create(size_t num_taps, float mu, float epsilon);

/**
 * @brief Xử lý một mẫu tín hiệu mới qua bộ lọc ANC NLMS.
 * 
 * @param ctx Con trỏ tới instance bộ lọc.
 * @param primary_sample Mẫu tín hiệu chính d(n) mang tín hiệu đích + nhiễu.
 * @param reference_sample Mẫu tín hiệu nhiễu tham chiếu x(n).
 * @return int32_t Mẫu tín hiệu sạch đầu ra e(n).
 */
int32_t anc_filter_process(anc_filter_t *ctx, int32_t primary_sample, int32_t reference_sample);

/**
 * @brief Giải phóng bộ nhớ của bộ lọc ANC NLMS.
 * 
 * @param ctx Con trỏ tới instance bộ lọc.
 */
void anc_filter_free(anc_filter_t *ctx);

/**
 * @brief Lấy chuẩn Euclid (weight norm ||w||) của bộ lọc thích nghi.
 * 
 * @param ctx Con trỏ tới instance bộ lọc.
 * @return float Chuẩn Euclid ||w||.
 */
float anc_filter_get_w_norm(const anc_filter_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // ANC_FILTER_H
