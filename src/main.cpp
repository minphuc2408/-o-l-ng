#include <Arduino.h>
#include "config.h"
#include "i2s_driver.h"
#include "cic_filter.h"
#include "anc_filter.h"
#include "heart_rate.h"
#include "serial_tx.h"

// Bộ đệm chứa các mẫu thô (48kHz) đọc từ I2S
static int32_t primary_buf[SAMPLE_BUFFER_SIZE];
static int32_t reference_buf[SAMPLE_BUFFER_SIZE];

// Instance bộ lọc hạ mẫu CIC cho kênh Primary (Left) và Reference (Right)
static cic_filter_t *cic_primary = NULL;
static cic_filter_t *cic_reference = NULL;

// Instance bộ lọc thích nghi ANC NLMS
static anc_filter_t *anc_ctx = NULL;

// FIFO riêng cho calibration: giữ nguyên int32_t CIC của cả hai kênh.
static int32_t calibration_primary_fifo[SAMPLE_BUFFER_SIZE * 2];
static int32_t calibration_reference_fifo[SAMPLE_BUFFER_SIZE * 2];
static size_t calibration_fifo_count = 0;
static bool calibration_active = false;
static uint32_t calibration_end_ms = 0;

static bool time_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

void setup() {
    // 1. Khởi tạo cổng truyền Serial (baud rate 921600, TX buffer 1024)
    serial_tx_init();

    // 2. Khởi tạo I2S Driver đọc 2 microphone (48kHz thô)
    esp_err_t err = i2s_driver_init();
    if (err != ESP_OK) {
        // Ghi lỗi ra Serial thô nếu lỗi khởi động
        Serial.printf("[SYSTEM ERROR] I2S Init failed: %d\n", err);
        while (1) {
            delay(1000);
        }
    }

    // 3. Khởi tạo bộ lọc CIC hạ mẫu bậc 2 cho kênh Primary và Reference
    cic_primary = cic_filter_create();
    cic_reference = cic_filter_create();
    if (cic_primary == NULL || cic_reference == NULL) {
        Serial.println("[SYSTEM ERROR] CIC Filter allocation failed!");
        while (1) {
            delay(1000);
        }
    }

    // 4. Khởi tạo bộ lọc thích nghi ANC NLMS
    anc_ctx = anc_filter_create(ANC_NUM_TAPS, ANC_MU_INIT, ANC_EPSILON);
    if (anc_ctx == NULL) {
        Serial.println("[SYSTEM ERROR] ANC Filter allocation failed!");
        while (1) {
            delay(1000);
        }
    }

    // 5. Khởi tạo trạng thái phát hiện BPM (bộ nhớ hoàn toàn tĩnh).
    heart_rate_reset();
}

void loop() {
    uint32_t requested_duration_ms = 0;
    if (serial_tx_poll_calibration_start(&requested_duration_ms)) {
        if (requested_duration_ms >= CALIBRATION_MIN_DURATION_MS &&
            requested_duration_ms <= CALIBRATION_MAX_DURATION_MS) {
            calibration_active = true;
            calibration_fifo_count = 0;
            calibration_end_ms = millis() + requested_duration_ms;
            // Không cho interval kéo dài xuyên qua khoảng tạm dừng measurement.
            heart_rate_reset();
        }
    }

    if (calibration_active && time_reached(millis(), calibration_end_ms)) {
        calibration_active = false;
        calibration_fifo_count = 0;
    }

    // Đọc một khối mẫu thô (48kHz) song song từ I2S (Primary ở kênh Trái, Reference ở kênh Phải)
    size_t read_pairs = i2s_driver_read(primary_buf, reference_buf, SAMPLE_BUFFER_SIZE);
    
    if (read_pairs > 0) {
        // Bộ đệm chứa mẫu đầu ra sau lọc hạ mẫu CIC (8kHz)
        static int32_t cic_primary_out[SAMPLE_BUFFER_SIZE];
        static int32_t cic_reference_out[SAMPLE_BUFFER_SIZE];
        
        // 1. Lọc hạ mẫu 48kHz -> 8kHz cho cả hai kênh Primary và Reference độc lập
        size_t cic_samples_p = cic_filter_process(cic_primary, primary_buf, read_pairs, cic_primary_out);
        size_t cic_samples_r = cic_filter_process(cic_reference, reference_buf, read_pairs, cic_reference_out);
        
        // Đảm bảo số lượng mẫu khớp nhau giữa 2 kênh
        size_t cic_samples = (cic_samples_p < cic_samples_r) ? cic_samples_p : cic_samples_r;
        
        if (calibration_active) {
            // Calibration xuất đúng input của ANC: hai kênh sau CIC, chưa ANC.
            for (size_t i = 0; i < cic_samples; i++) {
                if (calibration_fifo_count < (sizeof(calibration_primary_fifo) / sizeof(calibration_primary_fifo[0]))) {
                    calibration_primary_fifo[calibration_fifo_count] = cic_primary_out[i];
                    calibration_reference_fifo[calibration_fifo_count] = cic_reference_out[i];
                    calibration_fifo_count++;
                }
            }

            while (calibration_fifo_count >= CALIBRATION_FRAME_PAIRS) {
                serial_tx_send_calibration_frame(calibration_primary_fifo, calibration_reference_fifo,
                                                 CALIBRATION_FRAME_PAIRS);
                calibration_fifo_count -= CALIBRATION_FRAME_PAIRS;
                if (calibration_fifo_count > 0) {
                    memmove(calibration_primary_fifo, &calibration_primary_fifo[CALIBRATION_FRAME_PAIRS],
                            calibration_fifo_count * sizeof(int32_t));
                    memmove(calibration_reference_fifo, &calibration_reference_fifo[CALIBRATION_FRAME_PAIRS],
                            calibration_fifo_count * sizeof(int32_t));
                }
            }
            return;
        }

        // 2. Chạy ANC NLMS và đưa trực tiếp từng mẫu e(n) vào detector BPM.
        // Normal mode không truyền PCM; UART chỉ có dòng text khi phát hiện peak.
        for (size_t i = 0; i < cic_samples; i++) {
            // Lọc sạch nhiễu: e(n) = d(n) - wᵀ * x(n)
            int32_t clean_val = anc_filter_process(anc_ctx, cic_primary_out[i], cic_reference_out[i]);
            
            // Tín hiệu gốc của mic INMP441 ghi dạng 24-bit MSB-aligned trong 32-bit slot.
            // Ta dịch phải 14 bit để chuyển về dải int16_t một cách tối ưu.
            int32_t scaled_val = clean_val >> 14;
            
            // Saturation (Bão hòa chống tràn số khi ép kiểu)
            if (scaled_val > INT16_MAX) {
                scaled_val = INT16_MAX;
            } else if (scaled_val < INT16_MIN) {
                scaled_val = INT16_MIN;
            }

            // Xử lý tiếp tục từ e(n) ở đúng tốc độ đầu ra 8 kHz.
            processSample((int16_t)scaled_val);
        }
    } else {
        // Tránh chiếm dụng CPU vô ích nếu không có mẫu mới từ I2S
        delay(1);
    }
}
