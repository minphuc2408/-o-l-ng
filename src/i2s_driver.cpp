#include "i2s_driver.h"
#include "config.h"
#include "driver/i2s.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static QueueHandle_t i2s_event_queue = NULL;
static uint32_t dma_overflow_count = 0;

// Bộ đệm tĩnh chứa dữ liệu stereo interleaved [Left, Right, Left, Right...]
// Kích thước = SAMPLE_BUFFER_SIZE * 2 (mẫu)
static int32_t i2s_raw_buf[SAMPLE_BUFFER_SIZE * 2];

esp_err_t i2s_driver_init(void) {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE_RAW,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo: Left & Right
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 128,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        .mck_io_num = I2S_PIN_NO_CHANGE,
#endif
        .bck_io_num = PIN_SCK,
        .ws_io_num = PIN_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_SD
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 10, &i2s_event_queue);
    if (err != ESP_OK) {
        return err;
    }

    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        i2s_driver_uninstall(I2S_NUM_0);
        return err;
    }

    dma_overflow_count = 0;
    return ESP_OK;
}

size_t i2s_driver_read(int32_t *primary_buf, int32_t *reference_buf, size_t max_pairs) {
    if (primary_buf == NULL || reference_buf == NULL || max_pairs == 0) {
        return 0;
    }

    // Đảm bảo không vượt quá kích thước bộ đệm tĩnh của chúng ta
    size_t pairs_to_read = max_pairs;
    if (pairs_to_read > SAMPLE_BUFFER_SIZE) {
        pairs_to_read = SAMPLE_BUFFER_SIZE;
    }

    // Kiểm tra hàng đợi sự kiện để đếm lỗi DMA Overflow (Mẫu bị rớt)
    i2s_event_t event;
    while (i2s_event_queue != NULL && xQueueReceive(i2s_event_queue, &event, 0) == pdPASS) {
        // Cả hai enum có thể tồn tại tùy phiên bản SDK:
        // I2S_EVENT_RX_Q_OVF (khuyên dùng ở bản mới) hoặc I2S_EVENT_RX_OVERFLOW
#if defined(I2S_EVENT_RX_Q_OVF)
        if (event.type == I2S_EVENT_RX_Q_OVF) {
            dma_overflow_count++;
        }
#elif defined(I2S_EVENT_RX_OVERFLOW)
        if (event.type == I2S_EVENT_RX_OVERFLOW) {
            dma_overflow_count++;
        }
#endif
    }

    size_t bytes_to_read = pairs_to_read * 2 * sizeof(int32_t); // Mỗi cặp gồm Left và Right (32-bit/sample)
    size_t bytes_read = 0;

    // Đọc dữ liệu thô từ bus I2S
    // Sử dụng timeout hợp lý 100ms (100 / portTICK_PERIOD_MS)
    esp_err_t err = i2s_read(I2S_NUM_0, (void *)i2s_raw_buf, bytes_to_read, &bytes_read, 100 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        return 0;
    }

    size_t pairs_read = bytes_read / (2 * sizeof(int32_t));

    // Phân tách kênh (De-interleaving):
    // Dữ liệu đọc về dạng: [L0, R0, L1, R1, L2, R2...]
    // Do cấu hình phần cứng:
    // - Mic1 (L/R -> GND) ghi vào kênh Left (mẫu chẵn) -> Kênh Primary (nhiễu + tim/phổi)
    // - Mic2 (L/R -> VDD) ghi vào kênh Right (mẫu lẻ) -> Kênh Reference (chỉ nhiễu)
    for (size_t i = 0; i < pairs_read; i++) {
        primary_buf[i]   = i2s_raw_buf[i * 2];     // Chỉ số chẵn: Left Channel
        reference_buf[i] = i2s_raw_buf[i * 2 + 1]; // Chỉ số lẻ: Right Channel
    }

    return pairs_read;
}

uint32_t i2s_driver_get_overflow_count(void) {
    return dma_overflow_count;
}
