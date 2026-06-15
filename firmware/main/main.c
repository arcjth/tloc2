#include "jloc2.h"
#include "lib/tcp_debug.h"

static i2sBuffer buf;
static dbg_packet_t pkt;

void app_main(void) {
    wifi_debug_init();
    i2s_init();

    while (1) {
        
        bool ok    = i2s_start_capture(&buf);
        bool event = loc2d_detect(&buf);

        sndLoc2 loc = {0};

        if (event) {
            f64 r[3] = { buf.r_unit[0], buf.r_unit[1], buf.r_unit[2] };
            linSys3 sys;
            loc2d_build_tdoa_system(MIC_POS_UNIT, 0, r, &sys);
            loc2d_solve_tdoa(&sys, &loc);
        }

        pkt.flags    = (event ? DBG_FLAG_EVENT : 0) | (loc.valid ? DBG_FLAG_VALID : 0) | (ok ? DBG_FLAG_CAPOK : 0);
        pkt.ema[0]   = buf.ema[0]; pkt.ema[1] = buf.ema[1];
        pkt.ema[2]   = buf.ema[2]; pkt.ema[3] = buf.ema[3];
        pkt.r_unit[0] = buf.r_unit[0];
        pkt.r_unit[1] = buf.r_unit[1];
        pkt.r_unit[2] = buf.r_unit[2];
        pkt.loc_x    = (f32)loc.x;
        pkt.loc_y    = (f32)loc.y;
        pkt.loc_d_ref = (f32)loc.d_ref;

        wifi_debug_send(&pkt);  
    }
}
