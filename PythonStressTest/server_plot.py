import asyncio
import websockets
import json
import threading
import re
from datetime import datetime
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# Data structure to track packet loss for each node
# Format: node_id -> {"timestamps": [datetime], "loss_rate_history": [float]}
nodes_data = {}
history_length = 30  # Keep last 30 data points for plotting
ws_port = 8080
data_lock = threading.Lock()
verbose_incomplete_payloads = False
node_pattern = re.compile(r'"i"\s*:\s*"([^"]+)"')
packetloss_pattern = re.compile(r'"(?:packetloss|ketloss)"\s*:\s*(-?\d+(?:\.\d+)?)')
truncated_packetloss_pattern = re.compile(r':\s*(-?\d+(?:\.\d+)?)\s*,\s*"n"\s*:')

def extract_json_object(text):
    """Extract the first complete JSON object from text that may contain log prefixes."""
    start = text.find("{")
    if start == -1:
        raise json.JSONDecodeError("No JSON object found", text, 0)

    depth = 0
    in_string = False
    escape = False

    for index in range(start, len(text)):
        char = text[index]

        if escape:
            escape = False
            continue

        if char == "\\" and in_string:
            escape = True
            continue

        if char == '"':
            in_string = not in_string
            continue

        if in_string:
            continue

        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]

    raise json.JSONDecodeError("Incomplete JSON object", text, start)

def parse_packet(message):
    """Return the telemetry object, unwrapping uart_rx payload messages when needed."""
    if isinstance(message, bytes):
        message = message.decode("utf-8", errors="replace")

    data = json.loads(extract_json_object(message))

    if "payload" in data:
        payload = data["payload"]
        if isinstance(payload, str):
            return json.loads(extract_json_object(payload))
        if isinstance(payload, dict):
            return payload

    return data

def extract_json_objects_from_buffer(buffer):
    """Extract complete JSON objects from a stream buffer and return objects plus remaining text."""
    objects = []

    while True:
        start = buffer.find("{")
        if start == -1:
            return objects, buffer[-256:]

        if start > 0:
            buffer = buffer[start:]

        try:
            json_text = extract_json_object(buffer)
            objects.append(json.loads(json_text))
            buffer = buffer[len(json_text):]
        except json.JSONDecodeError:
            return objects, buffer[-4096:]

def unwrap_uart_rx(data, rx_buffer):
    """Return zero or more telemetry packets from a possibly fragmented uart_rx payload."""
    if "payload" not in data:
        return [data], rx_buffer

    payload = data["payload"]
    if isinstance(payload, dict):
        return [payload], rx_buffer
    if not isinstance(payload, str):
        return [], rx_buffer

    rx_buffer += payload
    packets, rx_buffer = extract_json_objects_from_buffer(rx_buffer)

    if not packets:
        fallback_packet = parse_truncated_uart_payload(payload)
        if fallback_packet:
            return [fallback_packet], rx_buffer

    return packets, rx_buffer

def parse_truncated_uart_payload(payload):
    """Recover packetloss from a truncated UART payload when node id and loss are still present."""
    node_match = node_pattern.search(payload)
    if not node_match:
        return None

    loss_match = packetloss_pattern.search(payload) or truncated_packetloss_pattern.search(payload)
    if not loss_match:
        return None

    return {
        "i": node_match.group(1),
        "packetloss": float(loss_match.group(1)),
    }

async def handle_client(websocket):
    print(f"Client connected: {websocket.remote_address}")
    rx_buffer = ""
    try:
        async for message in websocket:
            try:
                if isinstance(message, bytes):
                    message = message.decode("utf-8", errors="replace")

                data = json.loads(extract_json_object(message))
                packets, rx_buffer = unwrap_uart_rx(data, rx_buffer)

                if not packets and data.get("type") == "uart_rx":
                    if verbose_incomplete_payloads:
                        print(f"Waiting for complete UART JSON payload. Buffered {len(rx_buffer)} byte(s).")
                    continue

                for data in packets:
                    if "packetloss" not in data:
                        if data.get("type") == "uart_rx":
                            print(f"Warning: No 'packetloss' field found in UART data. Keys: {list(data.keys())}")
                        continue

                    node_id = data.get("i", "UnknownNode")
                    loss_rate = float(data["packetloss"])

                    with data_lock:
                        if node_id not in nodes_data:
                            nodes_data[node_id] = {"timestamps": [], "loss_rate_history": []}

                        node = nodes_data[node_id]
                        node["timestamps"].append(datetime.now())
                        node["loss_rate_history"].append(loss_rate)

                        if len(node["loss_rate_history"]) > history_length:
                            node["timestamps"].pop(0)
                            node["loss_rate_history"].pop(0)

            except json.JSONDecodeError:
                print(f"Invalid JSON received: {message}")
            except ValueError:
                print(f"Invalid packetloss value in data: {message}")
    except websockets.exceptions.ConnectionClosed:
        print(f"Client disconnected: {websocket.remote_address}")

def start_ws_server():
    """Run the asyncio event loop for the WebSocket server in a separate thread"""
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    start_server = websockets.serve(handle_client, "0.0.0.0", ws_port)
    loop.run_until_complete(start_server)
    print(f"WebSocket Server listening on ws://0.0.0.0:{ws_port}")
    loop.run_forever()

# --- Matplotlib Plotting ---
plt.style.use("seaborn-v0_8-darkgrid")
fig, ax = plt.subplots(figsize=(11, 6))
fig.patch.set_facecolor("#111827")
ax.set_facecolor("#172033")
ax.set_title("Real-time Packet Loss", fontsize=16, fontweight="bold", color="#F9FAFB", pad=14)
ax.set_xlabel(f"Sample (last {history_length} points)", color="#D1D5DB", labelpad=10)
ax.set_ylabel("Packet Loss (%)", color="#D1D5DB", labelpad=10)
ax.set_xlim(1, history_length)
ax.set_ylim(0, 100)
ax.grid(True, linestyle="--", linewidth=0.7, alpha=0.25, color="#CBD5E1")
ax.tick_params(axis="both", colors="#D1D5DB")
ax.set_xticks([1, 5, 10, 15, 20, 25, 30])

for spine in ax.spines.values():
    spine.set_color("#334155")

lines = {}

def update_plot(frame):
    with data_lock:
        snapshot = {
            node_id: {
                "timestamps": list(node["timestamps"]),
                "loss_rate_history": list(node["loss_rate_history"]),
            }
            for node_id, node in nodes_data.items()
        }

    for node_id, node in snapshot.items():
        if node_id not in lines:
            line, = ax.plot(
                [],
                [],
                label=node_id,
                linewidth=2.5,
                marker="o",
                markersize=4,
                markeredgewidth=0,
            )
            lines[node_id] = line
            legend = ax.legend(loc="upper left", facecolor="#111827", edgecolor="#334155")
            for text in legend.get_texts():
                text.set_color("#F9FAFB")
        
        y_data = node["loss_rate_history"]
        x_data = list(range(history_length - len(y_data) + 1, history_length + 1))
        lines[node_id].set_data(x_data, y_data)

    if snapshot:
        ax.set_xlim(1, history_length)
        # Auto-scale Y axis for better visibility if loss is small.
        current_max_loss = max(
            [max(n["loss_rate_history"]) if n["loss_rate_history"] else 0 for n in snapshot.values()] + [5.0]
        )
        ax.set_ylim(0, min(100, current_max_loss * 1.5))
        ax.set_title(
            f"Real-time Packet Loss - {len(snapshot)} node(s)",
            fontsize=16,
            fontweight="bold",
            color="#F9FAFB",
            pad=14,
        )

    return list(lines.values())

if __name__ == "__main__":
    print("Starting system...")
    
    # Start WebSocket server in a background daemon thread
    # so it doesn't block the Matplotlib GUI main thread.
    ws_thread = threading.Thread(target=start_ws_server, daemon=True)
    ws_thread.start()

    # Start Matplotlib animation
    print("Opening plot window...")
    ani = FuncAnimation(fig, update_plot, interval=500, blit=False, cache_frame_data=False)
    
    # plt.show() blocks the main thread until the window is closed
    plt.show()
    print("Plot window closed. Exiting.")
