#include <esp_log.h>
#include <dsps_ccorr.h>
#include <math.h>
#include "loc.h"

#define XCORR_BUF_LEN (I2S_MAX_SAMPLES * 2 - 1)

static void _from_channel(const i2sBuffer *buf, int ch, f32 *out) {
    for (int n = 0; n < I2S_MAX_SAMPLES; n++)
        out[n] = (f32)(buf->samples[n][ch] >> XCORR_SHIFT);
}

static f32 _rms(const f32 *buffer, int samples) {
    f32 acc = 0.0f;
    for (int i = 0; i < samples; i++) acc += buffer[i] * buffer[i];
    return sqrtf(acc / samples);
}

static int _matched_lag(const f32 *a, const f32 *b) {
    static f32 v[XCORR_BUF_LEN];
    dsps_ccorr_f32_ae32((float *)b, I2S_MAX_SAMPLES, (float *)a, I2S_MAX_SAMPLES, v);

    int mid = I2S_MAX_SAMPLES - 1;
    int lo  = mid - XCORR_MAX_LAG;
    int hi  = mid + XCORR_MAX_LAG;
    if (lo < 0) lo = 0;
    if (hi > XCORR_BUF_LEN - 1) hi = XCORR_BUF_LEN - 1;

    int lag = 0;
    f32 peak = v[lo];
    for (int i = lo + 1; i <= hi; i++) {
        if (v[i] > peak) {
            peak = v[i];
            lag = i - mid;
        }
    }
    return lag;
}

bool loc_detect(i2sBuffer *buf) {
    static f32 ref_buf[I2S_MAX_SAMPLES];
    static f32 cmp_buf[I2S_MAX_SAMPLES];

    bool event = false;
    for (int ch = 0; ch < I2S_CHANNELS; ch++) {
        _from_channel(buf, ch, cmp_buf);
        buf->ema[ch] = _rms(cmp_buf, I2S_MAX_SAMPLES);
        if (buf->ema[ch] > SND_THRES_AMP) event = true;
    }
    if (!event) return false;

    _from_channel(buf, 0, ref_buf);
    for (int i = 0; i < LOC_NOREF_CHANNELS; i++) {
        _from_channel(buf, i + 1, cmp_buf);
        int lag = _matched_lag(ref_buf, cmp_buf);
        buf->r_unit[i] = R_FROM_LAG(lag);
    }
    return true;
}

void loc_solve(const i2sBuffer *buf, sndLoc2 *loc) {
    linSys3 sys = {0};
    const int ref = 0;
    f64 xr = MIC_POS_UNIT[ref].i, yr = MIC_POS_UNIT[ref].j;
    f64 qr = xr * xr + yr * yr;

    int row = 0;
    for (int i = 0; i < 4; i++) {
        if (i == ref) continue;
        f64 xi = MIC_POS_UNIT[i].i, yi = MIC_POS_UNIT[i].j;
        f64 ri = buf->r_unit[row];
        f64 qi = xi * xi + yi * yi;
        sys.A[row][0] = 2.0 * (xi - xr);
        sys.A[row][1] = 2.0 * (yi - yr);
        sys.A[row][2] = 2.0 * ri;
        sys.b[row]    = qi - qr - ri * ri;
        row++;
    }

    lin3_gaussian_solve(&sys);
    if (!sys.solved) {
        loc->valid = false;
        return;
    }
    loc->x     = sys.x[0];
    loc->y     = sys.x[1];
    loc->d_ref = sys.x[2];
    loc->valid = (loc->d_ref >= 0.0);
}
