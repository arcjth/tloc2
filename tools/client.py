import socket
import struct
import os
import csv
from collections import deque
from time import sleep

import numpy as np
from rich.live import Live
from rich.layout import Layout
from rich.panel import Panel
from rich.text import Text

# --- network / protocol config ---
ESP32_IP = "192.168.4.1"
DBG_PORT = 3333
DBG_MAGIC = 0xBEEF1234
PACKET_SIZE = 224

WRITE_TO_CSV = True
CSV_FILE_PATH = "output/logs.csv"

HEADER_STRUCT = struct.Struct("<II10f")

# --- heatmap config ---
X_MIN, X_MAX = 0.0, 90.0
Y_MIN, Y_MAX = 0.0, 60.0
MIC_SPACING = 30.0
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
    r, g, b = int(rgb[0]), int(rgb[1]), int(rgb[2])
    return f"#{r:02x}{g:02x}{b:02x}"


class HeatmapVisualizer:
    def __init__(self):
        self.grid = np.zeros((PIXEL_ROWS, HEATMAP_COLS), dtype=np.float64)
        self.last_point_pixel = None  # (row, col) in grid space, for the marker

    def _to_grid(self, x, y):
        col = int((x - X_MIN) / (X_MAX - X_MIN) * (HEATMAP_COLS - 1))
        row = int((y - Y_MIN) / (Y_MAX - Y_MIN) * (PIXEL_ROWS - 1))
        return col, row

    def deposit(self, x, y):
        x = min(max(x, X_MIN), X_MAX)
        y = min(max(y, Y_MIN), Y_MAX)
        col, row = self._to_grid(x, y)

        radius = int(GAUSSIAN_SIGMA * 3)
        col_start, col_end = max(0, col - radius), min(HEATMAP_COLS, col + radius + 1)
        row_start, row_end = max(0, row - radius), min(PIXEL_ROWS, row + radius + 1)

        cols = np.arange(col_start, col_end)
        rows = np.arange(row_start, row_end)
        grid_col, grid_row = np.meshgrid(cols, rows)
        gaussian = np.exp(
            -(((grid_col - col) ** 2 + (grid_row - row) ** 2) / (2 * GAUSSIAN_SIGMA ** 2))
        )
        self.grid[row_start:row_end, col_start:col_end] += gaussian
        self.last_point_pixel = (row, col)

    def decay(self):
        self.grid *= DECAY_RATE

    def render(self) -> Panel:
        peak_value = self.grid.max()
        normalized = self.grid / peak_value if peak_value > 0 else self.grid

        # flip vertically: row 0 of the array is Y_MIN, but the top printed
        # line should show Y_MAX
        rgb = jet_colormap(normalized)[::-1]

        marker_row = marker_col = None
        if self.last_point_pixel is not None:
            row, col = self.last_point_pixel
            marker_row, marker_col = PIXEL_ROWS - 1 - row, col

        text = Text()
        for pixel_row in range(0, PIXEL_ROWS, 2):
            top = rgb[pixel_row]
            bottom = rgb[pixel_row + 1] if pixel_row + 1 < PIXEL_ROWS else rgb[pixel_row]
            for col in range(HEATMAP_COLS):
                top_rgb = top[col]
                bottom_rgb = bottom[col]
                if marker_row == pixel_row and marker_col == col:
                    top_rgb = (255, 255, 255)
                if marker_row == pixel_row + 1 and marker_col == col:
                    bottom_rgb = (255, 255, 255)
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


def process_packet(buffer: bytearray):
    global packet_info_text

    (
        _, flags,
        ema0, ema1, ema2, ema3,
        runit0, runit1, runit2,
        loc_x, loc_y, loc_dref
    ) = HEADER_STRUCT.unpack_from(buffer, 0)

    flags_bin = bin(flags)[2:]


    heatmap.decay()

    if loc_x != 0.000000 or loc_y != 0.000000:
        coord_history.append(f"X: {loc_x:.6f} | Y: {loc_y:.6f}")
        heatmap.deposit(loc_x, loc_y)

    history_display = "\n".join(coord_history) if coord_history else "[dim] nothing yet [/dim]"

    packet_info_text = (
        f"[cyan]flags:[/cyan]  {flags_bin}\n"
        f"[cyan]coords:[/cyan] ({loc_x:.6f}, {loc_y:.6f})\n"
        f"[cyan]d_ref:[/cyan]  {loc_dref:.6f}\n"
        f"[cyan]rms:[/cyan] [{ema0:.6f}, {ema1:.6f}, {ema2:.6f}, {ema3:.6f}]\n\n"
        f" history: \n"
        f"{history_display}\n\n"
        f"[dim]buffer remaining: {len(buffer) - PACKET_SIZE} bytes[/dim]"
    )

    logs.append(f"[bold green] found:[/bold green] X:{loc_x:.6f} Y:{loc_y:.6f}")

    if WRITE_TO_CSV and csv_writer:
        csv_writer.writerow([
            flags, ema0, ema1, ema2, ema3,
            runit0, runit1, runit2, loc_x, loc_y, loc_dref
        ])

    del buffer[:PACKET_SIZE]


def consume_buffer(buffer: bytearray):
    while len(buffer) >= PACKET_SIZE:
        magic_check, = struct.unpack_from("<I", buffer, 0)

        if magic_check == DBG_MAGIC:
            process_packet(buffer)
        else:
            logs.append(f"[red]mismatch while checking magic:[/red] 0x{magic_check:X}")
            del buffer[0]


def stream_data(live: Live):
    global connection_status

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((ESP32_IP, DBG_PORT))
        connection_status = f"[bold green]successfully connected to {ESP32_IP}:{DBG_PORT}[/bold green]"
        logs.append("[green]connection established.[/green]")
        live.update(generate_layout())

        buffer = bytearray()

        while True:
            chunk = sock.recv(2048)
            if not chunk:
                connection_status = "[bold red]connection closed by esp32.[/bold red]"
                logs.append("[red]socket closed.[/red]")
                live.update(generate_layout())
                break

            logs.append(f"[dim][RAW] got {len(chunk)} bytes[/dim]")

            buffer.extend(chunk)
            consume_buffer(buffer)
            live.update(generate_layout())


def main():
    global connection_status

    init_csv_logging()

    try:
        with Live(generate_layout(), refresh_per_second=10) as live:
            try:
                stream_data(live)
            except Exception as e:
                connection_status = f"[bold red]network error:[/bold red] {e}"
                logs.append(f"[bold red]Exception:[/bold red] {e}")
                live.update(generate_layout())
                sleep(5)
    finally:
        close_csv_logging()


if __name__ == "__main__":
    main()
