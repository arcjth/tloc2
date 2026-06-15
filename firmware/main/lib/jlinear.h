#ifndef JLINEAR
#define JLINEAR

// ================================================
// Módulo geral de sistema linear 3x3
// Ax = b
// ================================================

typedef struct vec2_t {
    double i, j;
} vec2;
typedef struct pol2_t {
    double mag, angle;
} pol2;

typedef struct linear_sys3_t {
    double A[3][3];
    double b[3];
    double x[3];          // solução final (x0, x1, x2)
    bool solved;
    double detA;          // det(A) debug
} linSys3;

static inline vec2 vec2_neg(vec2 vec);
static inline vec2 vec2_addsub(vec2 vec_a, vec2 vec_b);
static inline vec2 vec2_scale(vec2 vec, double k);
static inline vec2 polar2rect(pol2 vector);
static inline pol2 rect2polar(vec2 vector);
static inline pol2 pol2_mult(pol2 a, pol2 b);
static inline pol2 pol2_div(pol2 a, pol2 b);

void lin3_print(const char *nome, linSys3 *sys);
void lin3_gaussian_solve(linSys3 *sys);   // para mcu
void lin3_cramer_solve(linSys3 *sys);     // manual
double lin3_det(const double A[3][3]);    // determinante auxiliar

#endif