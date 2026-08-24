#ifndef LOC_H
#define LOC_H

#include "lib/i2s_esp.h"
#include "lib/math.h"

#define SND_SPEED     343.0f
#define SND_THRES_AMP 100.0f

#define XCORR_SHIFT   10
#define XCORR_MAX_LAG 80

#define R_FROM_LAG(lag) (SND_SPEED * ((f32)(lag) / I2S_SAMPLE_RATE))

typedef struct {
    f64  x, y, d_ref;
    bool valid;
} sndLoc2;

static const vec2 MIC_POS_UNIT[4] = {
    { 1.0,  0.0},
    { 0.0,  1.0},
    {-1.0,  0.0},
    { 0.0, -1.0}
};

bool loc_detect(i2sBuffer *buf);
void loc_solve(const i2sBuffer *buf, sndLoc2 *loc);

#endif
