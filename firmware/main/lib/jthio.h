#ifndef JTH_IOLIB
#define JTH_IOLIB

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;

#define COR0 "\033[0m"
#define COR1 "\033[34m"
#define COR2 "\033[36m"
#define COR3 "\033[35m"

#define MAX         0xFFFF
#define ioInvalid   "\nComando inválido.\a"
#define ioExit      "\nSaindo..."
#define ioSizeError "\nO valor máximo é %d."

char *get_input(char *input, int max);
f64   get_value(void);
u32   get_value_uint(u32 max);

#endif
