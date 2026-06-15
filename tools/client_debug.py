import socket, struct, threading, time
from collections import deque
import matplotlib.pyplot as plt
import matplotlib.animation as animation

HOST  = '192.168.4.1'
PORT  = 3333
FMT   = '<IIffffffffff'
SIZE  = struct.calcsize(FMT)   # 48 bytes
MAGIC = 0xBEEF1234

FLAG_EVENT = 1 << 0
FLAG_VALID = 1 << 1
FLAG_CAPOK = 1 << 2

latest  = None
xy_hist = deque(maxlen=80)

def recv_loop():
    global latest
    while True:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(5)
                s.connect((HOST, PORT))
                print(f"conectado a {HOST}:{PORT}")
                buf = b''
                while True:
                    buf += s.recv(256)
                    while len(buf) >= SIZE:
                        # sincroniza no magic
                        idx = buf.find(struct.pack('<I', MAGIC))
                        if idx < 0:
                            buf = buf[-(SIZE-1):]
                            break
                        if idx > 0:
                            buf = buf[idx:]
                            continue
                        if len(buf) < SIZE:
                            break
                        latest = struct.unpack(FMT, buf[:SIZE])
                        buf = buf[SIZE:]
        except Exception as e:
            print(f"reconectando... ({e})")
            time.sleep(1)

threading.Thread(target=recv_loop, daemon=True).start()


fig, (ax_ema, ax_r, ax_xy) = plt.subplots(1, 3, figsize=(15, 5))
fig.suptitle('TLOC2 — debug WiFi', fontsize=13)

# EMA
ax_ema.set_title('amplitude EMA por canal')
ax_ema.set_ylim(0, 3e6)
ax_ema.axhline(500000, color='red', linestyle='--', linewidth=1, label='threshold')
ax_ema.legend(fontsize=8)
bars_ema = ax_ema.bar(['ch0','ch1','ch2','ch3'], [0]*4, color='#5fb3b3')

# r_unit
ax_r.set_title('r_unit (TDOA normalizado)')
ax_r.set_ylim(-1.5, 1.5)
ax_r.axhline(0, color='gray', linewidth=0.8)
bars_r = ax_r.bar(['r1','r2','r3'], [0]*3, color='#b35fb3')

# XY
ax_xy.set_title('localização 2D')
ax_xy.set_xlim(-1.5, 1.5)
ax_xy.set_ylim(-1.5, 1.5)
ax_xy.set_aspect('equal')
ax_xy.grid(True, alpha=0.3)
ax_xy.scatter([1,0,-1,0], [0,1,0,-1], marker='^', s=120,
              color='#3399ff', zorder=5, label='mics')
scat = ax_xy.scatter([], [], s=40, c=[], cmap='YlOrRd',
                     vmin=0, vmax=80, alpha=0.7, label='fonte (histórico)')
ax_xy.legend(fontsize=8)

status = fig.text(0.5, 0.01, 'aguardando conexão...', ha='center', fontsize=10)

def update(_):
    if latest is None:
        return
    _, flags, e0,e1,e2,e3, r0,r1,r2, x,y,d = latest

    for bar, v in zip(bars_ema, [e0,e1,e2,e3]):
        bar.set_height(v)

    for bar, v in zip(bars_r, [r0,r1,r2]):
        bar.set_height(v)
        bar.set_color('#5fb3b3' if v >= 0 else '#b35fb3')

    if flags & FLAG_VALID:
        xy_hist.append((x, y))

    if xy_hist:
        xs, ys = zip(*xy_hist)
        scat.set_offsets(list(zip(xs, ys)))
        # cor pelo índice temporal: mais recente = mais vermelho
        scat.set_array(list(range(len(xy_hist))))
        scat.set_clim(0, max(len(xy_hist), 1))

    parts = []
    parts.append('EVENTO' if flags & FLAG_EVENT else 'silêncio')
    parts.append('loc válida' if flags & FLAG_VALID else 'loc inválida')
    parts.append('captura ok' if flags & FLAG_CAPOK else 'FALHA captura')
    if flags & FLAG_VALID:
        parts.append(f'({x:.3f}, {y:.3f}) m  d0={d:.3f} m')
    status.set_text('  |  '.join(parts))

ani = animation.FuncAnimation(fig, update, interval=80, cache_frame_data=False)
plt.tight_layout()
plt.show()
