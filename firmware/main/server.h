#ifndef SERVER_H
#define SERVER_H

#undef bool
#undef true
#undef false
#define bool char
#define true 1
#define false 0
#define u32 unsigned int
#define f32 float

#define SRV_PORT  3333
#define SRV_SSID  "TLOC2_JZZTHU"
#define SRV_PASS  "tloc2debug"
#define SRV_MAGIC 0xBEEF1234

#define SRV_FLAG_EVENT (1u << 0)
#define SRV_FLAG_VALID (1u << 1)
#define SRV_FLAG_CAPOK (1u << 2)

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
void server_init(void);
bool server_send(dbg_packet_t *pkt);
#else
static inline void server_init(void) {}
static inline bool server_send(dbg_packet_t *p) { (void)p; return false; }
#endif

#endif
