#define DBG_MAGIC 0xBEEF1234
#define DBG_PORT  3333

// flags bits
#define DBG_FLAG_EVENT  (1 << 0)
#define DBG_FLAG_VALID  (1 << 1)
#define DBG_FLAG_CAPOK  (1 << 2)

typedef struct {
    u32 magic;
    u32 flags;
    f32 ema[4];
    f32 r_unit[3];
    f32 loc_x;
    f32 loc_y;
    f32 loc_d_ref;
} dbg_packet_t;                  // 48 bytes, naturally aligned
