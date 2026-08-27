#ifndef I2S_ESP_H
#define I2S_ESP_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "hal/i2s_types.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"

#define i32 int
#define u32 unsigned int
#define f32 float

#define PIN_WS  GPIO_NUM_12
#define PIN_SCK GPIO_NUM_13
#define PIN_SD0 GPIO_NUM_2
#define PIN_SD1 GPIO_NUM_15

#define I2S_SAMPLE_RATE     48000
#define I2S_DMA_DESC        8
#define I2S_DMA_FRAMES      512
#define I2S_MAX_SAMPLES     1024
#define I2S_CHANNELS        4
#define LOC_NOREF_CHANNELS  (I2S_CHANNELS - 1)

#define I2S_DATA_BITS       24
#define I2S_SLOT_BITS       32
#define I2S_MCLK_MULT       ((I2S_SLOT_BITS % 32) ? 256 : 192)

#define I2S_CONVERT_SIGNED(raw) ((raw) >> (I2S_SLOT_BITS - I2S_DATA_BITS))

typedef struct {
    i32 samples[I2S_MAX_SAMPLES][I2S_CHANNELS];
    f32 ema[I2S_CHANNELS];
    f32 r_unit[LOC_NOREF_CHANNELS];
} i2sBuffer;

void i2s_init(void);
bool i2s_start_capture(i2sBuffer *buf);
void i2s_stop_capture(void);

#endif
