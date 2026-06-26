#ifndef JLOC_WIFIDEBUG
#define JLOC_WIFIDEBUG

#include "jthio.h"

#define DBG_MAGIC       0xBEEF1234
#define DBG_PORT        3333
#define DBG_SSID        "TLOC2_JZZTHU"
#define DBG_PASS        "tloc2debug"

#define DBG_FLAG_EVENT  (1u << 0)   // event detected
#define DBG_FLAG_VALID  (1u << 1)   // loc valid
#define DBG_FLAG_CAPOK  (1u << 2)   // I2S ok

typedef struct {
    u32 magic;
    u32 flags;
    f32 ema[4];
    f32 r_unit[3];
    f32 loc_x;
    f32 loc_y;
    f32 loc_d_ref;
} dbg_packet_t;

#ifdef DEBUG_WIFI
void wifi_debug_init(void);
bool wifi_debug_send(dbg_packet_t *pkt);
#else
static inline void wifi_debug_init(void) {}
static inline bool wifi_debug_send(dbg_packet_t *p) { (void)p; return false; }
#endif

#endif
