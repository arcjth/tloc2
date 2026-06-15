#ifndef JLOC2D
#define JLOC2D

#include "lib/jthio.h"
#include "lib/jlinear.h"
#include "lib/esp.h"

#define SND_SPEED     343.0f
#define SND_MIC_DIS   0.545f
#define SND_THRES_AMP 500000L
#define SND_EMA_RATE  0.1f
#define MIC_SCALE_M   0.550

#define XCORR_MAX_LAG 80    // max amostras de atraso físico: 0.545/343 * 48000 ≈ 76
#define XCORR_SHIFT   10    // evita overflow: i24 >> 10 ≈ 14-bit por amostra

static const char *str_loc_title   = "\n%s=== Localização 2D por TDOA (4 mics) ===%s\n";
static const char *str_loc_solved  = "%sFonte localizada em (%.4f, %.4f) m | d0 = %.4f m%s\n";
static const char *str_loc_invalid = "\n%sAVISO: possivelmente perto demais ou no meio dos microfones.\n%s\n";
static const char *str_loc_r_unit  = "%s(r1, r2, r3 em unidades) = (%.6f, %.6f, %.6f)%s\n";

typedef struct {
    f64  x, y;
    f64  d_ref;
    bool valid;
} sndLoc2;

static const vec2 MIC_POS_UNIT[4] = {
    { 1.0,  0.0},   // m0
    { 0.0,  1.0},   // m1
    {-1.0,  0.0},   // m2
    { 0.0, -1.0}    // m3
};

i16  xcorr_peak_lag(i2sBuffer *buf, int ch_ref, int ch_other);
bool loc2d_detect(i2sBuffer *buf);
void loc2d_build_tdoa_system(const vec2 mics[4], int ref_idx, const f64 r_unit[3], linSys3 *sys);
void loc2d_solve_tdoa(linSys3 *sys, sndLoc2 *loc);
f64  loc2d_r_from_delta_t(f64 delta_t_sec);
void loc2d_print_debug(i2sBuffer *buf);
void jloc_simulator(void);

#endif
