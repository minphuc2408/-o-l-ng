#include "cic_filter.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

// Định nghĩa cấu trúc bộ lọc CIC bậc 2
struct cic_filter_s {
    int64_t i1_state;       // Thanh ghi tích lũy Integrator 1
    int64_t i2_state;       // Thanh ghi tích lũy Integrator 2
    int64_t c1_state;       // Trạng thái trễ Comb 1 (lưu w[k-1])
    int64_t c2_state;       // Trạng thái trễ Comb 2 (lưu diff1[k-1])
    size_t decim_counter;   // Bộ đếm hạ mẫu (0 đến CIC_DECIMATION_FACTOR-1)
};

cic_filter_t* cic_filter_create(void) {
    cic_filter_t *ctx = (cic_filter_t *)malloc(sizeof(cic_filter_t));
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(cic_filter_t));
    }
    return ctx;
}

void cic_filter_free(cic_filter_t *ctx) {
    if (ctx != NULL) {
        free(ctx);
    }
}

size_t cic_filter_process(cic_filter_t *ctx, const int32_t *in, size_t in_len, int32_t *out) {
    if (ctx == NULL || in == NULL || out == NULL || in_len == 0) {
        return 0;
    }

    size_t out_idx = 0;

    for (size_t i = 0; i < in_len; i++) {
        // --- 1. TẦNG TÍCH LŨY (INTEGRATOR STAGES - Chạy ở 48kHz) ---
        // Sử dụng int64_t tránh tràn số khi cộng dồn liên tục ở tốc độ cao
        ctx->i1_state += in[i];
        ctx->i2_state += ctx->i1_state;

        // --- 2. HẠ MẪU (DOWNSAMPLING - Giữ 1 mẫu sau mỗi R mẫu) ---
        ctx->decim_counter++;
        if (ctx->decim_counter >= CIC_DECIMATION_FACTOR) {
            ctx->decim_counter = 0;

            // Lấy mẫu đầu ra từ tầng Integrator 2 làm đầu vào cho Comb 1
            int64_t w = ctx->i2_state;

            // --- 3. TẦNG HIỆU PHÂN (COMB STAGES - Chạy ở 8kHz) ---
            // Comb Stage 1: y1[k] = w[k] - w[k-1]
            int64_t diff1 = w - ctx->c1_state;
            ctx->c1_state = w; // Cập nhật trạng thái trễ Comb 1

            // Comb Stage 2: y2[k] = y1[k] - y1[k-1]
            int64_t diff2 = diff1 - ctx->c2_state;
            ctx->c2_state = diff1; // Cập nhật trạng thái trễ Comb 2

            // --- 4. CHUẨN HÓA ĐẦU RA (NORMALIZATION) ---
            // Độ lợi DC của bộ lọc CIC bậc N với hệ số hạ mẫu R và sai số M=1 là:
            // H_gain = (R * M)^N = (6 * 1)^2 = 36.
            // Chia cho 36 để đưa biên độ về khoảng hoạt động ban đầu của tín hiệu thô.
            int64_t normalized = diff2 / 36;

            // Bão hòa (Saturation) nếu vượt quá giới hạn 32-bit
            if (normalized > INT32_MAX) {
                normalized = INT32_MAX;
            } else if (normalized < INT32_MIN) {
                normalized = INT32_MIN;
            }

            // --- 5. BỘ LỌC BÙ SUY HAO (TODO: DROOP COMPENSATION) ---
            // TODO: [Droop Compensation Filter]
            // Bộ lọc CIC bậc 2 gây suy giảm dải cao gần tần số cắt (passband droop).
            // Nếu tần số từ 1500Hz đến 2000Hz bị suy giảm đáng kể làm mờ tiếng tim/phổi,
            // ta cần bổ sung một bộ lọc bù FIR (ví dụ 3-tap hoặc 5-tap) tại vị trí này
            // để làm phẳng đáp ứng tần số dải thông.
            
            out[out_idx++] = (int32_t)normalized;
        }
    }

    return out_idx;
}
