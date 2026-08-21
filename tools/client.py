import socket
import struct
import os
import csv
import subprocess
from collections import deque
from time import sleep

from rich.live import Live
from rich.layout import Layout
from rich.panel import Panel

ESP32_IP = "192.168.4.1"
DBG_PORT = 3333
DBG_MAGIC = 0xBEEF1234
PACKET_SIZE = 224

WRITE_TO_CSV = True
CSV_FILE_PATH = "output/logs.csv"

HEADER_STRUCT = struct.Struct("<II10f")

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

def generate_layout() -> Layout:

    layout = Layout()
    layout.split_column(Layout(name="header", size=3), Layout(name="body"))
    layout["body"].split_row(Layout(name="dashboard", ratio=1), Layout(name="logs", ratio=1))
    
    layout["header"].update(Panel(connection_status, style="bold white"))
    layout["dashboard"].update(Panel(packet_info_text, title="[bold green]last pkg, loc history", border_style="green"))
    
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

    if loc_x != 0.000000 or loc_y != 0.000000:
        coord_history.append(f"X: {loc_x:.6f} | Y: {loc_y:.6f}")

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
