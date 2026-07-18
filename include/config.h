#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/**
 * @file config.h
 * @brief Nguồn sự thật duy nhất cho các tham số cấu hình hệ thống.
 * Dự án: Hệ thống Ống nghe Kỹ thuật số Khử nhiễu Thích nghi trên ESP32.
 */

// --- Cấu hình lấy mẫu âm thanh (Audio Sampling) ---
#define SAMPLE_RATE_RAW         48000U  ///< Tần số lấy mẫu gốc từ microphone INMP441 (48 kHz)
#define SAMPLE_RATE_OUT         8000U   ///< Tần số lấy mẫu đầu ra sau khi qua bộ lọc hạ mẫu CIC (8 kHz)
#define CIC_DECIMATION_FACTOR   6U      ///< Hệ số hạ mẫu của CIC (SAMPLE_RATE_RAW / SAMPLE_RATE_OUT)
#define CIC_ORDER               2U      ///< Bậc của bộ lọc tích lũy-hiệu phân CIC (bậc 2)

// --- Cấu hình Buffer ---
#define SAMPLE_BUFFER_SIZE      512U    ///< Kích thước buffer mẫu xử lý (điểm cân bằng giữa latency và độ ổn định)

// --- Cấu hình Pinout I2S cho ESP32 (Stereo Bus) ---
#define PIN_WS                  21      ///< Chân Word Select (WS) / GPIO21
#define PIN_SCK                 22      ///< Chân Serial Clock (SCK) / GPIO22
#define PIN_SD                  23      ///< Chân Serial Data (SD) / GPIO23

// --- Giao tiếp Serial/UART ---
#define UART_BAUD_RATE          921600U ///< Tốc độ truyền nhận Serial (baud rate) qua cổng USB

// --- Phát hiện nhịp tim từ tín hiệu e(n) sau ANC ---
#define HEART_RATE_ENV_ALPHA                  0.08f
#define HEART_RATE_THRESHOLD_MULTIPLIER       1.5f
#define HEART_RATE_THRESHOLD_WINDOW_SAMPLES  SAMPLE_RATE_OUT
#define HEART_RATE_REFRACTORY_MS              300U
#define HEART_RATE_REFRACTORY_SAMPLES         ((SAMPLE_RATE_OUT * HEART_RATE_REFRACTORY_MS) / 1000U)
#define HEART_RATE_INTERVAL_MIN_MS            600U
#define HEART_RATE_INTERVAL_MAX_MS            1500U
#define HEART_RATE_BOOTSTRAP_INTERVAL_MS      900U
#define HEART_RATE_S2_REJECT_PERCENT          75U
#define HEART_RATE_INTERVAL_COUNT             8U

// --- Calibration qua USB ---
// Calibration stream dùng các mẫu CIC int32_t từ cả Primary và Reference.
#define CALIBRATION_MIN_DURATION_MS  10000UL
#define CALIBRATION_MAX_DURATION_MS  60000UL
#define CALIBRATION_FRAME_PAIRS      32U

// --- Kênh Microphone (Stereo Layout) ---
// Mic1 (L/R -> GND) = Left Channel  = Primary (Tín hiệu tim/phổi + nhiễu)
// Mic2 (L/R -> VDD) = Right Channel = Reference (Tín hiệu nhiễu môi trường)
#define I2S_CHANNEL_PRIMARY     0       ///< Kênh Primary (Left Channel)
#define I2S_CHANNEL_REFERENCE   1       ///< Kênh Reference (Right Channel)


// --- Cấu hình thuật toán khử nhiễu thích nghi ANC LMS/NLMS (EXP-001) ---
#define ANC_NUM_TAPS            64U      ///< Số lượng tap lọc thích nghi N (xác định qua cross-correlation)
#define ANC_MU_INIT             0.3f   ///< Hệ số bước học khởi tạo mu (µ) đảm bảo hội tụ và ổn định
#define ANC_EPSILON             0.0001f   ///< Hệ số epsilon tránh chia cho 0 khi công suất tham chiếu bằng 0

#endif // CONFIG_H
