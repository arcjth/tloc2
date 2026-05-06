#include <stdint.h>

// pinos
#define I2S_SCK   0x2   // sck global
#define I2S_WS    0x3   // ws global
#define I2S_SD0   0x4   // mic 0
#define I2S_SD1   0x5   // mic 1
#define I2S_SD2   0x6   // mic 2
#define I2S_SD3   0x7   // mic 3

// protocolo
#define I2S_CHANNELS        4
#define I2S_BITS_PER_CH     24
#define I2S_PERIOD_COUNT  1    // 1 ciclo por unidade
#define I2S_MAX_SAMPLES     96   // para caber na SRAM do Nano (2KB), não lembro qual foi a nossa dedução.

// capture_buffer
typedef struct {
    int32_t  samples[I2S_MAX_SAMPLES][I2S_CHANNELS];
    uint16_t num_samples;
    uint16_t fs;
} i2sBuffer;

static inline void _sck_high(void) { PORTD |= (1 << I2S_SCK); }
static inline void _sck_low(void)  { PORTD &= ~(1 << I2S_SCK);  }
static inline void _ws_high(void)  { PORTD |= (1 << I2S_WS); }
static inline void _ws_low(void)   { PORTD &= ~(1 << I2S_WS);  }

static inline void _ns_delay(void) { __asm__("nop\n\t"); delayMicroseconds(2); } // gasta 62.5ns

static inline void _read_sd(uint8_t out[I2S_CHANNELS]) {
    // agora deve ler em paralelo de VERDADE
    uint8_t pins = PIND;
    out[0] = (1 << I2S_SD0) & pins;
    out[1] = (1 << I2S_SD1) & pins;
    out[2] = (1 << I2S_SD2) & pins;
    out[3] = (1 << I2S_SD3) & pins;
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
    // totalmente desnecessario na teoria mas por paranoia:
    DDRD &= ~(1 << I2S_SD0);
    DDRD &= ~(1 << I2S_SD1);
    DDRD &= ~(1 << I2S_SD2);
    DDRD &= ~(1 << I2S_SD3);
    Serial.println("interface I2S incializada (24-bit, 4 canais)");
}

bool i2s_capture(i2sBuffer *buf) {
    if (!buf) return false;
    buf->fs       = 1562;

    for (uint16_t s = 0; s < buf->num_samples; s++) {

        int32_t raw[I2S_CHANNELS] = {0, 0, 0, 0};

        // ciclo 0: borda WS — MSB atrasado em 1 SCK (do datasheet)
        _ws_low();
        _sck_high(); 
        _ns_delay();
        _sck_low(); 

        // ciclos 1..24: bits, suco da leitura
        for (int bit = I2S_BITS_PER_CH - 1; bit >= 0; bit--) {
            uint8_t sd[I2S_CHANNELS];
            _sck_high();
            _ns_delay();
            _read_sd(sd); 
            _sck_low();
            for (int ch = 0; ch < I2S_CHANNELS; ch++)
                if (sd[ch]) raw[ch] |= (1L << bit); // coloca bits na posição certa sequencialmente
        }  _ns_delay();

        // ciclos 25..31: padding (sem dados, joga ciclos fora)
        for (int p = 0; p < (32 - 1 - I2S_BITS_PER_CH); p++) {
            _sck_high();
            _ns_delay();
            _sck_low();
            _ns_delay();
        }

        // canal RIGHT, completamente descartado (por enquanto)
        _ws_high();
        for (int p = 0; p < 32; p++) {
            _sck_high();
            _ns_delay();
            _sck_low();
            _ns_delay();
        }

        // extensão de sinal 24-bit → 32-bit e armazena
        for (int ch = 0; ch < I2S_CHANNELS; ch++)
            buf->samples[s][ch] = i2s_convert_24bit_signed(raw[ch]);
    }
    return true;
}

static i2sBuffer buf;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== TESTE I2S BIT-BANG || 4x INMP441 ===");
    i2s_init();
    buf.num_samples = I2S_MAX_SAMPLES;
}

void loop() {
    long t = millis();
    long t_f = 0;
    i2s_capture(&buf);
    t_f = millis();
    // Imprime todos os samples no formato:  s0_ch0,s0_ch1,s0_ch2,s0_ch3
    for (uint16_t s = 0; s < buf.num_samples; s++) {
        Serial.print("-1000000, ");
        Serial.print("1000000, ");
        for (int ch = 0; ch < I2S_CHANNELS; ch++) {
            Serial.print(buf.samples[s][ch]);
            if (ch < I2S_CHANNELS - 1) Serial.print(",");
        }
        Serial.println();
    } Serial.print("número de samples por segundo: ");
    Serial.println( (float)I2S_MAX_SAMPLES * 1000 / (t_f - t));
}
