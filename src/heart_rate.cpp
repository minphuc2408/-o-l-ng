#include "heart_rate.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"

namespace {

static_assert(HEART_RATE_THRESHOLD_WINDOW_SAMPLES > 0U,
              "Heart-rate threshold window must not be empty");
static_assert(HEART_RATE_INTERVAL_COUNT == 8U,
              "Median implementation requires exactly 8 intervals");
static_assert(HEART_RATE_INTERVAL_MIN_MS > HEART_RATE_REFRACTORY_MS,
              "Valid RR interval must be longer than the hard refractory period");
static_assert(HEART_RATE_INTERVAL_MAX_MS > HEART_RATE_INTERVAL_MIN_MS,
              "Maximum interval must be greater than minimum interval");
static_assert(HEART_RATE_BOOTSTRAP_INTERVAL_MS >= HEART_RATE_INTERVAL_MIN_MS &&
              HEART_RATE_BOOTSTRAP_INTERVAL_MS <= HEART_RATE_INTERVAL_MAX_MS,
              "Bootstrap RR interval must be inside the accepted range");
static_assert(HEART_RATE_S2_REJECT_PERCENT > 50U &&
              HEART_RATE_S2_REJECT_PERCENT < 100U,
              "S2 rejection ratio must be between 50 and 100 percent");

// Toàn bộ trạng thái nằm trong bộ nhớ tĩnh; không có malloc/new trong đường xử lý mẫu.
static float envelope_history[HEART_RATE_THRESHOLD_WINDOW_SAMPLES];
static uint32_t intervals_ms[HEART_RATE_INTERVAL_COUNT];

static float envelope = 0.0f;
static double envelope_sum = 0.0;
static uint32_t envelope_index = 0U;
static uint32_t envelope_count = 0U;

static uint64_t sample_index = 0U;
static uint64_t last_peak_sample = 0U;
static bool has_previous_peak = false;
static bool was_above_threshold = false;

static uint8_t interval_index = 0U;
static uint8_t interval_count = 0U;
static uint16_t current_bpm = 0U;

static void reset_interval_history(void) {
    memset(intervals_ms, 0, sizeof(intervals_ms));
    interval_index = 0U;
    interval_count = 0U;
    current_bpm = 0U;
}

static uint64_t calculate_median_twice_ms(uint8_t count) {
    uint32_t sorted[HEART_RATE_INTERVAL_COUNT];
    memcpy(sorted, intervals_ms, (size_t)count * sizeof(sorted[0]));

    // Tối đa 8 phần tử: thời gian cố định và không cấp phát bộ nhớ.
    for (uint8_t i = 1U; i < count; ++i) {
        const uint32_t value = sorted[i];
        uint8_t j = i;
        while (j > 0U && sorted[j - 1U] > value) {
            sorted[j] = sorted[j - 1U];
            --j;
        }
        sorted[j] = value;
    }

    if ((count & 1U) != 0U) {
        return (uint64_t)sorted[count / 2U] * 2ULL;
    }
    return (uint64_t)sorted[(count / 2U) - 1U] +
           (uint64_t)sorted[count / 2U];
}

static uint16_t calculate_bpm_from_intervals(void) {
    const uint64_t median_twice_ms =
        calculate_median_twice_ms(HEART_RATE_INTERVAL_COUNT);
    if (median_twice_ms == 0U) {
        return 0U;
    }

    // BPM = 60000 / median_ms = 120000 / median_twice_ms, làm tròn gần nhất.
    uint64_t bpm = (120000ULL + (median_twice_ms / 2ULL)) / median_twice_ms;
    if (bpm > UINT16_MAX) {
        bpm = UINT16_MAX;
    }
    return (uint16_t)bpm;
}

static uint32_t expected_interval_ms(void) {
    if (interval_count == 0U) {
        return HEART_RATE_BOOTSTRAP_INTERVAL_MS;
    }
    const uint64_t median_twice_ms =
        calculate_median_twice_ms(interval_count);
    return (uint32_t)((median_twice_ms + 1ULL) / 2ULL);
}

static uint32_t minimum_dynamic_interval_ms(void) {
    const uint32_t expected = expected_interval_ms();
    const uint32_t rhythm_minimum =
        (expected * HEART_RATE_S2_REJECT_PERCENT + 99U) / 100U;
    return (rhythm_minimum > HEART_RATE_INTERVAL_MIN_MS)
               ? rhythm_minimum
               : HEART_RATE_INTERVAL_MIN_MS;
}

static uint32_t envelope_amplitude(void) {
    if (envelope <= 0.0f) {
        return 0U;
    }
    if (envelope >= (float)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)(envelope + 0.5f);
}

static uint32_t interval_to_ms(uint64_t interval_samples) {
    const uint64_t rounded_ms =
        ((interval_samples * 1000ULL) + (SAMPLE_RATE_OUT / 2U)) / SAMPLE_RATE_OUT;
    return (rounded_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)rounded_ms;
}

static void register_peak(uint64_t peak_sample, uint32_t amplitude) {
    uint32_t interval_ms = 0U;

    if (has_previous_peak) {
        const uint64_t interval_samples = peak_sample - last_peak_sample;
        interval_ms = interval_to_ms(interval_samples);

        if (interval_ms >= HEART_RATE_INTERVAL_MIN_MS &&
            interval_ms <= HEART_RATE_INTERVAL_MAX_MS) {
            intervals_ms[interval_index] = interval_ms;
            interval_index = (uint8_t)((interval_index + 1U) % HEART_RATE_INTERVAL_COUNT);
            if (interval_count < HEART_RATE_INTERVAL_COUNT) {
                ++interval_count;
            }

            if (interval_count == HEART_RATE_INTERVAL_COUNT) {
                current_bpm = calculate_bpm_from_intervals();
            }
        } else {
            // Khoảng ngắt quá dài báo hiệu mất chuỗi peak liên tục. Không giữ
            // BPM cũ vì nó không còn đại diện cho 8 interval gần nhất.
            reset_interval_history();
        }
    }

    last_peak_sample = peak_sample;
    has_previous_peak = true;

    // Tần suất ghi thấp (chỉ khi có peak); không ghi trong ISR.
    Serial.printf("Peak detected | interval=%lums | amplitude=%lu | BPM=%u\n",
                  (unsigned long)interval_ms,
                  (unsigned long)amplitude,
                  (unsigned int)current_bpm);
}

} // namespace

void heart_rate_reset(void) {
    memset(envelope_history, 0, sizeof(envelope_history));
    reset_interval_history();

    envelope = 0.0f;
    envelope_sum = 0.0;
    envelope_index = 0U;
    envelope_count = 0U;
    sample_index = 0U;
    last_peak_sample = 0U;
    has_previous_peak = false;
    was_above_threshold = false;
}

void processSample(int16_t e_n) {
    const uint64_t current_sample = sample_index++;
    const int32_t signed_sample = (int32_t)e_n;
    const float magnitude =
        (float)((signed_sample < 0) ? -signed_sample : signed_sample);

    envelope = (HEART_RATE_ENV_ALPHA * magnitude) +
               ((1.0f - HEART_RATE_ENV_ALPHA) * envelope);

    const float expired_envelope = envelope_history[envelope_index];
    envelope_history[envelope_index] = envelope;
    envelope_sum += (double)envelope - (double)expired_envelope;

    ++envelope_index;
    if (envelope_index >= HEART_RATE_THRESHOLD_WINDOW_SAMPLES) {
        envelope_index = 0U;
    }
    if (envelope_count < HEART_RATE_THRESHOLD_WINDOW_SAMPLES) {
        ++envelope_count;
    }

    // Chờ đủ đúng cửa sổ 1 giây để tránh ngưỡng thấp giả trong giai đoạn khởi động.
    if (envelope_count < HEART_RATE_THRESHOLD_WINDOW_SAMPLES) {
        was_above_threshold = false;
        return;
    }

    const double moving_average =
        envelope_sum / (double)HEART_RATE_THRESHOLD_WINDOW_SAMPLES;
    const double adaptive_threshold =
        moving_average * (double)HEART_RATE_THRESHOLD_MULTIPLIER;
    const bool is_above_threshold = (double)envelope > adaptive_threshold;

    // Refractory cứng loại rung nội bộ; khóa RR động bên dưới loại S2/nhiễu
    // mà không cập nhật last_peak_sample, nên mốc S1 gần nhất được giữ nguyên.
    if (is_above_threshold && !was_above_threshold) {
        const uint32_t amplitude = envelope_amplitude();
        if (!has_previous_peak) {
            register_peak(current_sample, amplitude);
        } else {
            const uint64_t candidate_samples = current_sample - last_peak_sample;
            if (candidate_samples >= HEART_RATE_REFRACTORY_SAMPLES) {
                const uint32_t candidate_interval_ms =
                    interval_to_ms(candidate_samples);
                const uint32_t required_interval_ms =
                    minimum_dynamic_interval_ms();

                if (candidate_interval_ms >= required_interval_ms ||
                    candidate_interval_ms > HEART_RATE_INTERVAL_MAX_MS) {
                    register_peak(current_sample, amplitude);
                } else {
                    // Không cập nhật last_peak_sample: candidate kế tiếp vẫn
                    // được đo từ S1 đã chấp nhận gần nhất.
                    Serial.printf(
                        "Candidate rejected | interval=%lums | amplitude=%lu | reason=S2\n",
                        (unsigned long)candidate_interval_ms,
                        (unsigned long)amplitude);
                }
            }
        }
    }

    was_above_threshold = is_above_threshold;
}

uint16_t heart_rate_get_bpm(void) {
    return current_bpm;
}
