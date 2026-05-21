#include <stdint.h>

// pinos
#define I2S_SCK   0x2   // sck global
#define I2S_WS    0x3   // ws global
#define I2S_SD0   0x4   // mic 0
#define I2S_SD1   0x5   // mic 1
#define I2S_SD2   0x6   // mic 2
#define I2S_SD3   0x7   // mic 3

// protocolo
#define I2S_CHANNELS     4
#define I2S_NOREF_CHANNELS 3
#define I2S_BITS_PER_CH 24
#define I2S_MAX_LAG 4   
#define I2S_MAX_SAMPLES 64
// quantas vezes dividir por dois no xcrr, que é necessário pra que não haja overflow de 32-bits.
// existe um sweet-spot baseado nos valores maximos que os mics enviam, que parece ser de 10 bits.
#define I2S_OVERFLOW_BIT_MARGIN 10

#define SND_SPEED 343.0f
#define SND_MIC_DIS 0.545f
#define SND_THRES_AMP 500000L
#define SND_EMA_RATE 0.1f

#define NS_DELAY __asm__("nop\n\t")

// capture_buffer
typedef struct {
    int32_t samples[I2S_MAX_SAMPLES][I2S_CHANNELS];
    float ema[I2S_CHANNELS];
    float r_unit[I2S_NOREF_CHANNELS];
} i2sBuffer;

// quando detecta um evento sonoro
#define BIT_EVENT_EMA b0

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

static inline void _sck_high(void) { PORTD |= (1 << I2S_SCK); }
static inline void _sck_low(void)  { PORTD &= ~(1 << I2S_SCK);  }
static inline void _ws_high(void)  { PORTD |= (1 << I2S_WS); }
static inline void _ws_low(void)   { PORTD &= ~(1 << I2S_WS);  }

// static inline void _ns_delay(void) { __asm__("nop\n\t"); } // gasta 62.5ns

static inline void _read_sd(uint8_t out[I2S_CHANNELS]) {
    // agora deve ler em paralelo de VERDADE
    uint8_t pins = PIND;
    out[0] = (pins >> I2S_SD0) & 1;
    out[1] = (pins >> I2S_SD1) & 1;
    out[2] = (pins >> I2S_SD2) & 1;
    out[3] = (pins >> I2S_SD3) & 1;
}

static inline int32_t i2s_convert_24bit_signed(int32_t raw) {
    if (raw & (1L << 23)) {           // se o bit de sinal está 1
        return raw | 0xFF000000;    // adiciona o restante de 1's
    }
    return raw & 0x00FFFFFF;        // mascara 24 bits
}

// I2S bit-bang — inicialização
void i2s_init(void) {
    // pinos 2 e 3 como output, não precisa manipular os do 4 a 7
    // pois por padrão os pinos estão como input
    DDRD |= (1 << I2S_SCK);
    DDRD |= (1 << I2S_WS);
    // inicializa SCK e WS em zero:
    PORTD &= ~(1 << I2S_SCK);
    PORTD &= ~(1 << I2S_WS);
    Serial.println("interface I2S incializada (24-bit, 4 canais)");
}

// produto interno entre mic de referência e outro canal, em um dado lag
// positivo = ch_other chegou depois (mais longe da fonte)
static int32_t _xcorr_at_lag(i2sBuffer *buf, int ch_ref, int ch_other, int16_t lag) {
    int32_t acc = 0;
    for (uint16_t n = I2S_MAX_LAG; n < I2S_MAX_SAMPLES - I2S_MAX_LAG; n++) {
        acc += (buf->samples[n][ch_ref] >> I2S_OVERFLOW_BIT_MARGIN) * (buf->samples[n + lag][ch_other] >> I2S_OVERFLOW_BIT_MARGIN);
    }
    return acc;
}

// retorna o lag de máxima correlação entre ch_ref e ch_other
int16_t xcorr_peak_lag(i2sBuffer *buf, int ch_ref, int ch_other) {
    int16_t best_lag = 0;
    int32_t best_val = INT32_MIN; 
    for (int16_t lag = -I2S_MAX_LAG; lag <= I2S_MAX_LAG; lag++) {
        int32_t val = _xcorr_at_lag(buf, ch_ref, ch_other, lag);
        if (val > best_val) {
            best_val = val;
            best_lag = lag;
        }
    } return best_lag;
}

// lag em samples, delta_t em segundos: retorna r_i pro solver gaussiano.
static float lag_to_r(int16_t lag, float sampleRate) {
    float delta_t = (float)lag / sampleRate;
    return SND_SPEED * delta_t / SND_MIC_DIS;
}

void has_something_happened(i2sBuffer *buf, float sampleRate, byteMask *byte) {
    byte->BIT_EVENT_EMA = false;
    for (uint8_t i = 0; i < 3; i++) {
        buf->r_unit[i] = 0.0f;
    }
    for (int8_t ch = 0; ch < I2S_CHANNELS; ch++)
        if (buf->ema[ch] > 0.5f) { byte->BIT_EVENT_EMA = true; break; }
    if (!byte->BIT_EVENT_EMA) return;
    for (uint8_t i = 0; i < 3; i++) {
        buf->r_unit[i] = lag_to_r(xcorr_peak_lag(buf, 0, (i+1)), sampleRate);
    }
}

void i2s_capture(i2sBuffer *buf) {
    for (uint16_t s = 0; s < I2S_MAX_SAMPLES; s++) {
        int32_t raw[I2S_CHANNELS] = {0, 0, 0, 0};
        //_pick_sample(raw);
        _ws_low();
        _sck_high(); NS_DELAY;
        _sck_low();

        // LEFT
        for (int bit = I2S_BITS_PER_CH - 1; bit >= 0; bit--) {
            uint8_t sd[I2S_CHANNELS];
            _sck_high(); NS_DELAY;
            _read_sd(sd);
            _sck_low();
            // sem delay aqui devido ao loop de gravação.
            for (int ch = 0; ch < I2S_CHANNELS; ch++)
                if (sd[ch]) raw[ch] |= (1L << bit);
        }
        // PADDING
        for (int p = 0; p < (32 - 1 - I2S_BITS_PER_CH); p++) {
            _sck_high(); NS_DELAY;
            _sck_low(); NS_DELAY;
        }
        // RIGHT
        _ws_high();
        for (int p = 0; p < 32; p++) {
            _sck_high(); NS_DELAY;
            _sck_low(); NS_DELAY;
        }
        // extensão de sinal 24-bit → 32-bit e armazena
        for (int ch = 0; ch < I2S_CHANNELS; ch++) {
            buf->samples[s][ch] = i2s_convert_24bit_signed(raw[ch]);
            buf->ema[ch] = buf->ema[ch] * (1.0f - SND_EMA_RATE) + (abs(buf->samples[s][ch]) > SND_THRES_AMP) * SND_EMA_RATE;
        }
    }
}

static i2sBuffer buf;
static byteMask logic_byte;

void basicDebug(i2sBuffer *buf, long *processTime, long *sampleRate, byteMask *byte) {
    for (uint16_t s = 0; s < I2S_MAX_SAMPLES; s++) {
        Serial.print(SND_THRES_AMP);  Serial.print(",");
        Serial.print(-SND_THRES_AMP); Serial.print(",");
        for (int ch = 0; ch < I2S_CHANNELS; ch++) {
            Serial.print(buf->samples[s][ch]);
            if (ch < I2S_CHANNELS - 1) Serial.print(",");
        }
        Serial.println();
    }
    if (byte->BIT_EVENT_EMA) {
        Serial.print("# r=");
        Serial.print(buf->r_unit[0]); Serial.print(",");
        Serial.print(buf->r_unit[1]); Serial.print(",");
        Serial.println(buf->r_unit[2]);
    } else { Serial.println("# NO_EVENT"); }
    Serial.print("# processTime="); Serial.print(*processTime);
    Serial.print("ms sampleRate="); Serial.print(*sampleRate);
    Serial.println("sps");
}

void setup() {
    Serial.begin(115200);
    Serial.println("=== TLOC2 || TDOA ===");
    i2s_init();
}

void loop() {
    long initTime = millis();
    i2s_capture(&buf);
    long processTime = millis() - initTime;
    long sampleRate = (1000L * I2S_MAX_SAMPLES) / processTime;
    has_something_happened(&buf, (float)sampleRate, &logic_byte);
    basicDebug(&buf, &processTime, &sampleRate, &logic_byte);
}
