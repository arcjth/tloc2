#include "jloc2.h"
#include <stdio.h>

static i64 _xcorr_at_lag(i2sBuffer *buf, int ch_ref, int ch_other, i16 lag) {
    i64 acc = 0;
    for (u16 n = XCORR_MAX_LAG; n < I2S_MAX_SAMPLES - XCORR_MAX_LAG; n++) {
        acc += (i64)(buf->samples[n][ch_ref]         >> XCORR_SHIFT)
             * (i64)(buf->samples[n + lag][ch_other] >> XCORR_SHIFT);
    }
    return acc;
}


i16 xcorr_peak_lag(i2sBuffer *buf, int ch_ref, int ch_other) {
    i16 best_lag = 0;
    i64 best_val = INT64_MIN;
    for (i16 lag = -XCORR_MAX_LAG; lag <= XCORR_MAX_LAG; lag++) {
        i64 val = _xcorr_at_lag(buf, ch_ref, ch_other, lag);
        if (val > best_val) { best_val = val; best_lag = lag; }
    }
    return best_lag;
}


static inline i32 _abs32(i32 x) { return x < 0 ? -x : x; }

bool loc2d_detect(i2sBuffer *buf) {
    for (int ch = 0; ch < I2S_CHANNELS; ch++) {
        i64 sum = 0;
        for (int s = 0; s < I2S_MAX_SAMPLES; s++)
            sum += _abs32(buf->samples[s][ch]);
        f32 amp = (f32)(sum / I2S_MAX_SAMPLES);
        buf->ema[ch] = SND_EMA_RATE * amp + (1.0f - SND_EMA_RATE) * buf->ema[ch];
    }

    bool event = false;
    for (int ch = 0; ch < I2S_CHANNELS; ch++)
        if (buf->ema[ch] > SND_THRES_AMP) { event = true; break; }
    if (!event) return false;

    for (int i = 0; i < LOC_NOREF_CHANNELS; i++)
        buf->r_unit[i] = (f32)loc2d_r_from_delta_t(
            (f64)xcorr_peak_lag(buf, 0, i + 1) / I2S_SAMPLE_RATE
        );

    return true;
}


f64 loc2d_r_from_delta_t(f64 delta_t_sec) {
    return SND_SPEED * delta_t_sec / MIC_SCALE_M;
}

void loc2d_build_tdoa_system(const vec2 mics[4], int ref_idx, const f64 r_unit[3], linSys3 *sys) {
    printf(str_loc_title, COR2, COR0);
    int row = 0;
    f64 xr = mics[ref_idx].i, yr = mics[ref_idx].j;
    f64 qr = xr*xr + yr*yr;
    for (int i = 0; i < 4; i++) {
        if (i == ref_idx) continue;
        f64 xi = mics[i].i, yi = mics[i].j;
        f64 ri = r_unit[row];
        f64 qi = xi*xi + yi*yi;
        sys->A[row][0] = 2.0 * (xi - xr);
        sys->A[row][1] = 2.0 * (yi - yr);
        sys->A[row][2] = 2.0 * ri;
        sys->b[row]    = qi - qr - ri*ri;
        row++;
    }
    sys->solved = false;
    lin3_print("Sistema TDOA 3x3", sys);
}

void loc2d_solve_tdoa(linSys3 *sys, sndLoc2 *loc) {
    lin3_gaussian_solve(sys);
    if (!sys->solved) {
        printf(str_loc_invalid, COR3, COR0);
        loc->valid = false;
        return;
    }
    loc->x     = sys->x[0] * MIC_SCALE_M;
    loc->y     = sys->x[1] * MIC_SCALE_M;
    loc->d_ref = sys->x[2] * MIC_SCALE_M;
    loc->valid = (loc->d_ref >= 0.0);
    printf(loc->valid ? str_loc_solved : str_loc_invalid,
           COR2, loc->x, loc->y, loc->d_ref, COR0);
}


void loc2d_print_debug(i2sBuffer *buf) {
    for (u16 s = 0; s < I2S_MAX_SAMPLES; s++) {
        printf("%ld,%ld", (long)SND_THRES_AMP, (long)-SND_THRES_AMP);
        for (int ch = 0; ch < I2S_CHANNELS; ch++)
            printf(",%ld", (long)buf->samples[s][ch]);
        printf("\n");
    }
    printf("# ema=%.0f,%.0f,%.0f,%.0f | r=%.4f,%.4f,%.4f\n",
           buf->ema[0], buf->ema[1], buf->ema[2], buf->ema[3],
           buf->r_unit[0], buf->r_unit[1], buf->r_unit[2]);
}
