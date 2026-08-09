#include "esp.h"

static const char *TAG = "TLOC2";

static i2s_chan_handle_t rx0, rx1;
static i32 tmp[I2S_MAX_SAMPLES * 2];    // scratch estéreo para desentrelaçamento

static i2s_chan_handle_t _i2s_create(int port, i2s_role_t role, gpio_num_t ws, gpio_num_t sd) {
    ESP_LOGI(TAG, "creating i2s channel %d (%s), ws=%d sd=%d bclk=%d",
             port, role == I2S_ROLE_MASTER ? "master" : "slave", ws, sd, PIN_SCK);
    ESP_LOGI(TAG, "  sample_rate=%dHz data_bits=%d slot_bits=%d mclk_mult=%d",
             I2S_SAMPLE_RATE, I2S_DATA_BITS, I2S_SLOT_BITS, I2S_MCLK_MULT);

    i2s_chan_handle_t rx;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(port, role);
    chan_cfg.dma_desc_num  = I2S_DMA_DESC;
    chan_cfg.dma_frame_num = I2S_DMA_FRAMES;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = {
            .sample_rate_hz  = I2S_SAMPLE_RATE,
            .clk_src         = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple   = I2S_MCLK_MULT,
            .bclk_div        = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_24BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode      = I2S_SLOT_MODE_STEREO,
            .slot_mask      = I2S_STD_SLOT_BOTH,
            .ws_width       = 32,
            .ws_pol         = false,
            .bit_shift      = true,
            .msb_right      = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_SCK,
            .ws   = ws,
            .dout = I2S_GPIO_UNUSED,
            .din  = sd,
            .invert_flags = { .bclk_inv = false, .mclk_inv = false, .ws_inv = false },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &std_cfg));
    return rx;
}

void i2s_init(void) {
    ESP_LOGI(TAG, "=== TLOC2 || TDOA ===");
    rx0 = _i2s_create(I2S_NUM_0, I2S_ROLE_MASTER, PIN_WS, PIN_SD0);
    rx1 = _i2s_create(I2S_NUM_1, I2S_ROLE_SLAVE,  PIN_WS, PIN_SD1);
    // slave bound to master's mclk making it fully synced
    ESP_ERROR_CHECK(i2s_channel_enable(rx1));   
    ESP_ERROR_CHECK(i2s_channel_enable(rx0));
}

i32 i2s_convert_signed(i32 raw) {
    return raw >> (I2S_SLOT_BITS - I2S_DATA_BITS);
}

bool i2s_start_capture(i2sBuffer *buf) {
    size_t bytes_read;
    const size_t read_bytes = I2S_MAX_SAMPLES * 2 * sizeof(i32);
    bool ok = true;

    if (i2s_channel_read(rx0, tmp, read_bytes, &bytes_read, portMAX_DELAY) != ESP_OK) ok = false;
    for (int s = 0; s < I2S_MAX_SAMPLES; s++) {
        buf->samples[s][0] = i2s_convert_signed(tmp[s * 2]);
        buf->samples[s][1] = i2s_convert_signed(tmp[s * 2 + 1]);
    }

    if (i2s_channel_read(rx1, tmp, read_bytes, &bytes_read, portMAX_DELAY) != ESP_OK) ok = false;
    for (int s = 0; s < I2S_MAX_SAMPLES; s++) {
        buf->samples[s][2] = i2s_convert_signed(tmp[s * 2]);
        buf->samples[s][3] = i2s_convert_signed(tmp[s * 2 + 1]);
    }
    return ok;
}

void i2s_stop_capture(void) {
    i2s_channel_disable(rx0);
    i2s_channel_disable(rx1);
}
