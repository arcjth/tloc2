#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "driver/i2s_std.h"

static const char *TAG = "TLOC2";

#define PIN_WS GPIO_NUM_4
#define PIN_SCK GPIO_NUM_18
#define PIN_SD0 GPIO_NUM_33
#define PIN_SD1 GPIO_NUM_35

#define I2S_CHANNELS          4
#define I2S_STEREO_CHANNELS   2
#define I2S_NOREF_CHANNELS    3
#define I2S_MAX_LAG           4
#define I2S_MAX_SAMPLES     1024
#define I2S_OVERFLOW_BIT_MARGIN 10

// acoustics
#define SND_SPEED        343.0f
#define SND_MIC_DIS      0.545f
#define SND_THRES_AMP    500000L
#define SND_EMA_RATE     0.1f

// adjust depending on the CPU clock
#define NS_DELAY __asm__ volatile( \
    "nop\nnop\nnop\nnop\nnop\nnop\n" \
    "nop\nnop\nnop\nnop\nnop\nnop\n")

// main system struct
typedef struct {
    int32_t samples[I2S_MAX_SAMPLES][I2S_CHANNELS];
    float   ema[I2S_CHANNELS];
    float   r_unit[I2S_NOREF_CHANNELS];
} i2sBuffer;

// useless micro optimization
#define BIT_EVENT_EMA     b0
#define BIT_EVENT_FAIL0   b1
#define BIT_EVENT_FAIL1   b2

typedef struct {
    uint8_t b0 : 1;
    uint8_t b1 : 1;
    uint8_t b2 : 1;
    uint8_t b3 : 1;
    uint8_t b4 : 1;
    uint8_t b5 : 1;
    uint8_t b6 : 1;
    uint8_t b7 : 1;
} byteMask;

static inline int32_t _abs32(int32_t x) { return x < 0 ? -x : x; }

i2s_chan_handle_t i2s(int sample_rate, int ws, int sd) {
    i2s_chan_handle_t rx;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_24BIT,
            .slot_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = 32,
            .ws_pol = false,
            .bit_shift = true,
            .msb_right = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_SCK,
            .ws = ws,
            .dout = I2S_GPIO_UNUSED,
            .din = sd,
            .invert_flags = {
                .bclk_inv = false,
                .mclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &std_cfg));

    return rx;
}
/*

void i2s_read(i2sBuffer *buf, float sampleRate, int8_t *byte) {
    i2s_chan_handle_t rx = i2s(sampleRate, I2S_WS, I2S_SD0)
    byte->BIT_EVENT_FAIL0 = i2s_channel_read(rx, buf->samples)
    byte->BIT_EVENT_FAIL1 = i2s_channel_read(rx, buf->samples)
}
   
// cross correlaction
// produto escalar entre ch_ref e ch_other com deslocamento lag
// positivo = ch_other chegou depois (mais longe da fonte)
static int32_t _xcorr_at_lag(i2sBuffer *buf, int ch_ref, int ch_other, int16_t lag) {
    int32_t acc = 0;
    for (uint16_t n = I2S_MAX_LAG; n < I2S_MAX_SAMPLES - I2S_MAX_LAG; n++) {
        acc += (buf->samples[n][ch_ref]         >> I2S_OVERFLOW_BIT_MARGIN)
             * (buf->samples[n + lag][ch_other] >> I2S_OVERFLOW_BIT_MARGIN);
    }
    return acc;
}

// peak cross correlaction
int16_t xcorr_peak_lag(i2sBuffer *buf, int ch_ref, int ch_other) {
    int16_t best_lag = 0;
    int32_t best_val = INT32_MIN;
    for (int16_t lag = -I2S_MAX_LAG; lag <= I2S_MAX_LAG; lag++) {
        int32_t val = _xcorr_at_lag(buf, ch_ref, ch_other, lag);
        if (val > best_val) {
            best_val = val;
            best_lag = lag;
        }
    }
    return best_lag;
}

static float lag_to_r(int16_t lag, float sampleRate) {
    float delta_t = (float)lag / sampleRate;
    return SND_SPEED * delta_t / SND_MIC_DIS;
}

// event detection + R_i calculation
void has_something_happened(i2sBuffer *buf, float sampleRate, byteMask *byte) {
    byte->BIT_EVENT_EMA = 0;
    for (uint8_t i = 0; i < I2S_NOREF_CHANNELS; i++)
        buf->r_unit[i] = 0.0f;

    for (int8_t ch = 0; ch < I2S_CHANNELS; ch++) {
        if (buf->ema[ch] > 0.5f) { byte->BIT_EVENT_EMA = 1; break; }
    }
    if (!byte->BIT_EVENT_EMA) return;

    for (uint8_t i = 0; i < I2S_NOREF_CHANNELS; i++)
        buf->r_unit[i] = lag_to_r(xcorr_peak_lag(buf, 0, (int)(i + 1)), sampleRate);
}

void basicDebug(i2sBuffer *buf, int64_t *processTime_us, long *sampleRate, byteMask *byte) {
    for (uint16_t s = 0; s < I2S_MAX_SAMPLES; s++) {
        printf("%ld,%ld", (long)SND_THRES_AMP, (long)-SND_THRES_AMP);
        for (int ch = 0; ch < I2S_CHANNELS; ch++) {
            printf(",%ld", (long)buf->samples[s][ch]);
        }
        printf("\n");
    }

    if (byte->BIT_EVENT_EMA) {
        printf("# r=%.4f,%.4f,%.4f\n",
               buf->r_unit[0], buf->r_unit[1], buf->r_unit[2]);
    } else {
        printf("# NO_EVENT\n");
    }

    printf("# processTime=%" PRId64 "ms sampleRate=%ldsps\n",
           *processTime_us / 1000, *sampleRate);
}

static i2sBuffer buf;
static byteMask  logic_byte;

void app_main(void) {
    ESP_LOGI(TAG, "=== TLOC2 || TDOA ===");
    i2s_init();

    while (1) {
        int64_t init_us = esp_timer_get_time();
        i2s_capture(&buf);
        int64_t process_us = esp_timer_get_time() - init_us;
        long    sample_rate = (1000000LL * I2S_MAX_SAMPLES) / process_us;

        has_something_happened(&buf, (float)sample_rate, &logic_byte);
        basicDebug(&buf, &process_us, &sample_rate, &logic_byte);
    }
}

