#include "anc_filter.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Định nghĩa cấu trúc bộ lọc thích nghi ANC NLMS
struct anc_filter_s {
    size_t num_taps;          // Số lượng tap lọc N
    float mu;                 // Hệ số bước học µ
    float epsilon;            // Hệ số epsilon tránh chia cho 0
    float *w;                 // Vector trọng số w[N]
    float *x_history;         // Vector trễ tín hiệu tham chiếu x_history[N]
};

// Hằng số nhân để chuyển đổi nhanh int32_t thành float trong dải [-1.0, 1.0]
// 1.0f / 2147483648.0f = 4.656612873077393e-10f
static const float INT32_TO_FLOAT_SCALE = 4.656612873077393e-10f;

anc_filter_t* anc_filter_create(size_t num_taps, float mu, float epsilon) {
    if (num_taps == 0) {
        return NULL;
    }

    anc_filter_t *ctx = (anc_filter_t *)malloc(sizeof(anc_filter_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->num_taps = num_taps;
    ctx->mu = mu;
    ctx->epsilon = epsilon;

    // Cấp phát và khởi tạo bộ nhớ về 0 cho vector trọng số và lịch sử tín hiệu
    ctx->w = (float *)calloc(num_taps, sizeof(float));
    ctx->x_history = (float *)calloc(num_taps, sizeof(float));

    if (ctx->w == NULL || ctx->x_history == NULL) {
        if (ctx->w != NULL) free(ctx->w);
        if (ctx->x_history != NULL) free(ctx->x_history);
        free(ctx);
        return NULL;
    }

    return ctx;
}

int32_t anc_filter_process(anc_filter_t *ctx, int32_t primary_sample, int32_t reference_sample) {
    if (ctx == NULL || ctx->w == NULL || ctx->x_history == NULL) {
        return primary_sample;
    }

    // 1. Chuẩn hóa mẫu tín hiệu đầu vào từ int32_t sang float [-1.0, 1.0]
    float d = (float)primary_sample * INT32_TO_FLOAT_SCALE;
    float x = (float)reference_sample * INT32_TO_FLOAT_SCALE;

    // 2. Dịch chuyển hàng đợi trễ tham chiếu x_history (chèn reference_sample mới vào đầu)
    for (size_t i = ctx->num_taps - 1; i > 0; i--) {
        ctx->x_history[i] = ctx->x_history[i - 1];
    }
    ctx->x_history[0] = x;

    // 3. Tính toán ước lượng nhiễu (y_hat = wᵀ * x)
    float y_hat = 0.0f;
    for (size_t i = 0; i < ctx->num_taps; i++) {
        y_hat += ctx->w[i] * ctx->x_history[i];
    }

    // 4. Tính toán sai số (e = d - y_hat) -> đây chính là tín hiệu ra sạch
    float e = d - y_hat;

    // 5. Tính toán năng lượng tín hiệu tham chiếu (norm_x = ||x(n)||^2 + epsilon)
    float norm_x = 0.0f;
    for (size_t i = 0; i < ctx->num_taps; i++) {
        norm_x += ctx->x_history[i] * ctx->x_history[i];
    }
    float norm_x_eps = norm_x + ctx->epsilon;

    // 6. Cập nhật trọng số thích nghi (w(n+1) = w(n) + [mu / (||x(n)||^2 + epsilon)] * e * x(n))
    float step = (ctx->mu / norm_x_eps) * e;
    for (size_t i = 0; i < ctx->num_taps; i++) {
        ctx->w[i] += step * ctx->x_history[i];
    }

    // 7. Chuyển đổi và bão hòa tín hiệu ra sạch e(n) về dải int32_t.
    // Không ghi log theo từng mẫu; normal UART chỉ phát sự kiện BPM theo peak.
    float e_scaled = e * 2147483648.0f;
    int32_t out_e;
    if (e_scaled >= 2147483647.0f) {
        out_e = INT32_MAX;
    } else if (e_scaled <= -2147483648.0f) {
        out_e = INT32_MIN;
    } else {
        out_e = (int32_t)e_scaled;
    }

    return out_e;
}

void anc_filter_free(anc_filter_t *ctx) {
    if (ctx != NULL) {
        if (ctx->w != NULL) {
            free(ctx->w);
        }
        if (ctx->x_history != NULL) {
            free(ctx->x_history);
        }
        free(ctx);
    }
}

float anc_filter_get_w_norm(const anc_filter_t *ctx) {
    if (ctx == NULL || ctx->w == NULL) {
        return 0.0f;
    }
    float w_norm = 0.0f;
    for (size_t i = 0; i < ctx->num_taps; i++) {
        w_norm += ctx->w[i] * ctx->w[i];
    }
    return sqrtf(w_norm);
}
