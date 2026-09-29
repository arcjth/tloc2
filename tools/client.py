import socket
import struct
import os
import csv
import threading
import time
from collections import deque

import numpy as np
from rich.live import Live
from rich.layout import Layout
from rich.panel import Panel
from rich.text import Text

# --- network / protocol config ---
ESP32_IP = "192.168.4.1"
DBG_PORT = 3333
DBG_MAGIC = 0xBEEF1234

HEADER_STRUCT = struct.Struct("<II10f")  # 4 + 4 + (10 * 4) = 48 bytes
PACKET_SIZE = HEADER_STRUCT.size

WRITE_TO_CSV = True
CSV_FILE_PATH = "output/logs.csv"

X_MIN, X_MAX = -0.1, 0.1
Y_MIN, Y_MAX = -0.1, 0.1
MIC_SPACING = 10.5
MIC_POSITIONS = [(i * MIC_SPACING, 0.0) for i in range(4)]

HEATMAP_COLS = 70
HEATMAP_ROWS = 24
PIXEL_ROWS = HEATMAP_ROWS * 2
GAUSSIAN_SIGMA = 1.5
DECAY_RATE = 0.95

logs = deque(maxlen=15)
coord_history = deque(maxlen=20)
packet_info_text = "[dim]waiting for first valid packet...[/dim]"
connection_status = f"[yellow]connecting to esp32 @{ESP32_IP}:{DBG_PORT}...[/yellow]"

csv_file = None
csv_writer = None
csv_lock = threading.Lock()


def init_csv_logging():
    global csv_file, csv_writer
    if not WRITE_TO_CSV:
        return

    os.makedirs(os.path.dirname(CSV_FILE_PATH), exist_ok=True)
    file_exists = os.path.isfile(CSV_FILE_PATH)

    csv_file = open(CSV_FILE_PATH, mode='a', newline='')
    csv_writer = csv.writer(csv_file)

    if not file_exists:
        csv_writer.writerow([
            "flags", "ema0", "ema1", "ema2", "ema3",
            "runit0", "runit1", "runit2", "loc_x", "loc_y", "loc_dref"
        ])


def close_csv_logging():
    if csv_file:
        csv_file.close()


def jet_colormap(values: np.ndarray) -> np.ndarray:
    """Map a normalized [0, 1] array to jet-like RGB, shape (..., 3) uint8."""
    v = np.clip(values, 0.0, 1.0)
    r = np.clip(np.minimum(4 * v - 1.5, -4 * v + 4.5), 0, 1)
    g = np.clip(np.minimum(4 * v - 0.5, -4 * v + 3.5), 0, 1)
    b = np.clip(np.minimum(4 * v + 0.5, -4 * v + 2.5), 0, 1)
    rgb = np.stack([r, g, b], axis=-1)
    return (rgb * 255).astype(np.uint8)


def rgb_to_hex(rgb) -> str:
    return f"#{int(rgb[0]):02x}{int(rgb[1]):02x}{int(rgb[2]):02x}"


class HeatmapVisualizer:
    def __init__(self):
        self.grid = np.zeros((PIXEL_ROWS, HEATMAP_COLS), dtype=np.float64)
        self.last_point_pixel = None
        self.lock = threading.Lock()

        # Precompute the 2D Gaussian kernel once to eliminate runtime matrix generation overhead
        radius = int(GAUSSIAN_SIGMA * 3)
        self.radius = radius
        y, x = np.ogrid[-radius:radius + 1, -radius:radius + 1]
        self.kernel = np.exp(-(x**2 + y**2) / (2 * GAUSSIAN_SIGMA**2))

    def _to_grid(self, x, y):
        col = int((x - X_MIN) / (X_MAX - X_MIN) * (HEATMAP_COLS - 1))
        row = int((y - Y_MIN) / (Y_MAX - Y_MIN) * (PIXEL_ROWS - 1))
        return col, row

    def deposit(self, x, y):
        x = min(max(x, X_MIN), X_MAX)
        y = min(max(y, Y_MIN), Y_MAX)
        col, row = self._to_grid(x, y)

        with self.lock:
            r = self.radius
            c_start, c_end = max(0, col - r), min(HEATMAP_COLS, col + r + 1)
            r_start, r_end = max(0, row - r), min(PIXEL_ROWS, row + r + 1)

            kc_start = r - (col - c_start)
            kc_end = r + (c_end - col)
            kr_start = r - (row - r_start)
            kr_end = r + (r_end - row)

            self.grid[r_start:r_end, c_start:c_end] += self.kernel[kr_start:kr_end, kc_start:kc_end]
            self.last_point_pixel = (row, col)

    def render(self) -> Panel:
        with self.lock:
            self.grid *= DECAY_RATE
            peak_value = self.grid.max()
            normalized = self.grid / peak_value if peak_value > 0 else self.grid
            rgb = jet_colormap(normalized)[::-1]
            marker_pixel = self.last_point_pixel

        marker_row = marker_col = None
        if marker_pixel is not None:
            row, col = marker_pixel
            marker_row, marker_col = PIXEL_ROWS - 1 - row, col

        text = Text()
        for pixel_row in range(0, PIXEL_ROWS, 2):
            top = rgb[pixel_row]
            bottom = rgb[pixel_row + 1] if pixel_row + 1 < PIXEL_ROWS else rgb[pixel_row]
            for col in range(HEATMAP_COLS):
                top_rgb = (255, 255, 255) if (marker_row == pixel_row and marker_col == col) else top[col]
                bottom_rgb = (255, 255, 255) if (marker_row == pixel_row + 1 and marker_col == col) else bottom[col]
                text.append("▀", style=f"{rgb_to_hex(top_rgb)} on {rgb_to_hex(bottom_rgb)}")
            if pixel_row + 2 < PIXEL_ROWS:
                text.append("\n")

        return Panel(text, title="[bold green]sound source heatmap", border_style="green")


heatmap = HeatmapVisualizer()


def generate_layout() -> Layout:
    layout = Layout()
    layout.split_column(Layout(name="header", size=3), Layout(name="body"))
    layout["body"].split_row(Layout(name="heatmap", ratio=2), Layout(name="sidebar", ratio=1))
    layout["sidebar"].split_column(Layout(name="dashboard"), Layout(name="logs"))

    layout["header"].update(Panel(connection_status, style="bold white"))
    layout["heatmap"].update(heatmap.render())
    layout["dashboard"].update(Panel(packet_info_text, title="[bold cyan]last packet", border_style="cyan"))

    log_output = "\n".join(logs) if logs else "[dim] nothing yet [/dim]"
    layout["logs"].update(Panel(log_output, title="[bold blue] event stream", border_style="blue"))

    return layout


def process_packet(data: bytes):
    global packet_info_text

    if len(data) < PACKET_SIZE:
        return

    magic_check, flags, ema0, ema1, ema2, ema3, runit0, runit1, runit2, loc_x, loc_y, loc_dref = HEADER_STRUCT.unpack(data[:PACKET_SIZE])

    if magic_check != DBG_MAGIC:
        logs.append(f"[red]magic mismatch:[/red] 0x{magic_check:X}")
        return

    flags_bin = bin(flags)[2:]

    if loc_x != 0.0 or loc_y != 0.0:
        coord_history.append(f"X: {loc_x:.6f} | Y: {loc_y:.6f}")
        heatmap.deposit(loc_x, loc_y)

    history_display = "\n".join(coord_history) if coord_history else "[dim] nothing yet [/dim]"

    packet_info_text = (
        f"[cyan]flags:[/cyan]  {flags_bin}\n"
        f"[cyan]coords:[/cyan] ({loc_x:.6f}, {loc_y:.6f})\n"
        f"[cyan]d_ref:[/cyan]  {loc_dref:.6f}\n"
        f"[cyan]rms:[/cyan] [{ema0:.6f}, {ema1:.6f}, {ema2:.6f}, {ema3:.6f}]\n\n"
        f" history: \n"
        f"{history_display}\n"
    )

    logs.append(f"[bold green] found:[/bold green] X:{loc_x:.6f} Y:{loc_y:.6f}")

    if WRITE_TO_CSV and csv_writer:
        with csv_lock:
            csv_writer.writerow([
                flags, ema0, ema1, ema2, ema3,
                runit0, runit1, runit2, loc_x, loc_y, loc_dref
            ])


def network_thread(stop_event):
    global connection_status

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)

    try:
        sock.sendto(b"PING", (ESP32_IP, DBG_PORT))
        connection_status = f"[bold green]UDP stream active @ {ESP32_IP}:{DBG_PORT}[/bold green]"
        logs.append("[green]Sent UDP initialization packet.[/green]")
    except Exception as e:
        connection_status = f"[bold red]Socket error:[/bold red] {e}"
        return

    while not stop_event.is_set():
        try:
            data, _ = sock.recvfrom(1024)
            process_packet(data)
        except socket.timeout:
            try:
                sock.sendto(b"PING", (ESP32_IP, DBG_PORT))
            except Exception:
                pass
        except Exception as e:
            logs.append(f"[bold red]UDP Receive Error:[/bold red] {e}")
            time.sleep(0.1)

    sock.close()


def main():
    global connection_status

    init_csv_logging()
    stop_event = threading.Event()

    net_thread = threading.Thread(target=network_thread, args=(stop_event,), daemon=True)
    net_thread.start()

    try:
        with Live(generate_layout(), refresh_per_second=15) as live:
            while True:
                live.update(generate_layout())
                time.sleep(1 / 15)
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        close_csv_logging()


if __name__ == "__main__":
    main()
