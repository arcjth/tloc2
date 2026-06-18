#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "jthio.h"

static const char *str_i2s_init = "%sI2S interface initialized (24-bit, %d channels)%s\n";
static const char *str_i2s_capture = "%sCapture started - waiting for trigger...%s\n";

char *get_input(char *input, int max) {
    putchar('\n');
    fgets(input, max, stdin);
    input[strcspn(input, "\n")] = '\0';
    return input;
}

/**
* @brief returns a double precision value, supports negatives, zero, 
* non dot decimals (ex: EU, BR notations), cientific notation, etc.
* for non-negative values, please use get_uvalue()
 */
f64 get_value(void) {
    const char *conversion_error = "\nUm erro pode ter ocorrido na conversão, verifique o valor inserido \n(dica: utilize not. científica (ex: 1,7E-9 = 1,7 nano, 1,0e6 = 1 milhão)).";
    char input[MAX];
    double value = 0.0;
    get_input(input, MAX);
    for (char *pinput = input; *pinput != '\0'; pinput++) {
        if (*pinput == ',') *pinput = '.'; // pra permitir a notação brasileira, europeia ou internacional
    }
    value = atof(input);
    return value;
}
/** 
* @brief same as get_value() but returns an unsigned integer, 
* for positive double precision floats, use get_uvalue.
* set @param[in] max to -1 to disable it.
*/
u32 get_uvalue(size_t max) {
    unsigned int uvalue = 0;
    char input[MAX];
    // could be a u32 directly, but negative values would break
    i64 value = atoi(get_input(input, MAX));
    if (value <= 0 ) {
        printf("%s", ioInvalid);
    } else if (max >= 1 && value > max) {
        printf(ioSizeError, max);
    } else uvalue = (u32)value;
    return uvalue;
}

int32_t i2s_convert_24bit_signed(int32_t raw) {
    return raw >> 8;
}
