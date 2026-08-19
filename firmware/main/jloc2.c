#include "jloc2.h"
#include "lib/potors/snd.h"
#include <stdio.h>

#define XCORR_EVAL "xcorr_eval_log:"

i64 _xcorr_at_lag(i2sBuffer *buf, int ch_ref, int ch_other, i16 lag) {
    i64 acc = 0;
    for (u16 n = XCORR_MAX_LAG; n < I2S_MAX_SAMPLES - XCORR_MAX_LAG; n++) {
        acc += (i64)(buf->samples[n][ch_ref]         >> XCORR_SHIFT)
             * (i64)(buf->samples[n + lag][ch_other] >> XCORR_SHIFT);
    }
    return acc;
}

static void _from_channel(i2sBuffer *buf, u8 ch, f32 *out) {
    for (u16 n = 0; n < I2S_MAX_SAMPLES; n++) {
        out[n] = (f32)(buf->samples[n][ch] >> XCORR_SHIFT);
        if (n > 20) continue; 
    }
}

i16 xcorr_peak_lag(i2sBuffer *buf, int ch_ref, int ch_other) {
    static float a[I2S_MAX_SAMPLES];
    static float b[I2S_MAX_SAMPLES];
    _from_channel(buf,   ch_ref, a);
    _from_channel(buf, ch_other, b);
    match_t m = snd_matched_filter(a, b, I2S_MAX_SAMPLES);
    if (m.lag > XCORR_MAX_LAG) {
      ESP_LOGI(XCORR_EVAL, "exceded max permitted lag\n");
    }
    return (i16)m.lag;
}

static inline i32 _abs32(i32 x) { return x < 0 ? -x : x; }

bool loc2d_detect(i2sBuffer *buf) {
    static float ref_buf[I2S_MAX_SAMPLES];
    static float cmp_buf[I2S_MAX_SAMPLES];

    float rms[I2S_CHANNELS];
    for (int ch = 0; ch < I2S_CHANNELS; ch++) {
        _from_channel(buf, ch, cmp_buf); 
        rms[ch] = snd_rms(cmp_buf, I2S_MAX_SAMPLES);
        printf("%f \n", rms[ch]);
        buf->ema[ch] = rms[ch];
    }

    bool event = false;
    for (int ch = 0; ch < I2S_CHANNELS; ch++)
        if (buf->ema[ch] > SND_THRES_AMP) { event = true; break; }
    if (!event) return false;

    _from_channel(buf, 0, ref_buf); 
    for (int i = 0; i < LOC_NOREF_CHANNELS; i++) {
        _from_channel(buf, i + 1, cmp_buf);
        match_t m = snd_matched_filter(ref_buf, cmp_buf, I2S_MAX_SAMPLES);
        buf->r_unit[i] = (f32)loc2d_r_from_delta_t((f32)m.lag / I2S_SAMPLE_RATE);
    }
    return true;
}

static inline f32 loc2d_r_from_delta_t(f32 delta_t_sec) {
    return SND_SPEED * delta_t_sec;
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
        //printf(str_loc_invalid, COR3, COR0);
        loc->valid = false;
        return;
    }
    loc->x     = sys->x[0] ;// * MIC_SCALE_M;
    loc->y     = sys->x[1] ;// * MIC_SCALE_M;
    loc->d_ref = sys->x[2] ;// * MIC_SCALE_M;
    loc->valid = (loc->d_ref >= 0.0);
    //printf(loc->valid ? str_loc_solved : str_loc_invalid,
         //  COR2, loc->x, loc->y, loc->d_ref, COR0);
    printf("%f %f\n", loc->x, loc->y);
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
