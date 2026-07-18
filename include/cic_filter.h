#ifndef CIC_FILTER_H
#define CIC_FILTER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cic_filter_s cic_filter_t;

/**
 * @brief Tạo mới một bộ lọc CIC bậc 2.
 * @return cic_filter_t* Con trỏ tới instance bộ lọc mới, hoặc NULL nếu thất bại.
 */
cic_filter_t* cic_filter_create(void);

/**
 * @brief Giải phóng bộ nhớ của bộ lọc CIC.
 * @param ctx Con trỏ tới bộ lọc cần giải phóng.
 */
void cic_filter_free(cic_filter_t *ctx);

/**
 * @brief Xử lý lọc hạ mẫu bằng CIC bậc 2 (48kHz -> 8kHz).
 * @param ctx Con trỏ tới instance bộ lọc.
 * @param in Buffer dữ liệu đầu vào (48kHz).
 * @param in_len Số lượng mẫu đầu vào.
 * @param out Buffer ghi kết quả đầu ra (8kHz).
 * @return size_t Số lượng mẫu thực tế được ghi vào buffer `out`.
 */
size_t cic_filter_process(cic_filter_t *ctx, const int32_t *in, size_t in_len, int32_t *out);

#ifdef __cplusplus
}
#endif

#endif // CIC_FILTER_H
