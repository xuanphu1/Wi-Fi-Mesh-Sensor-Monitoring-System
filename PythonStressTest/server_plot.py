import asyncio
import websockets
import json
import threading
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# Data structure to track packet loss for each node
# Format: node_id -> {"expected_seq": int, "lost_packets": int, "total_packets": int, "loss_rate_history": [float]}
nodes_data = {}
history_length = 50  # Keep last 50 data points for plotting

async def handle_client(websocket):
    print(f"Client connected: {websocket.remote_address}")
    try:
        async for message in websocket:
            try:
                data = json.loads(message)
                node_id = data.get("i", "UnknownNode")
                
                # Use the pre-calculated packetloss from the C firmware if available
                if "packetloss" in data:
                    loss_rate = float(data["packetloss"])
                    
                    if node_id not in nodes_data:
                        nodes_data[node_id] = {"loss_rate_history": []}
                    
                    node = nodes_data[node_id]
                    node["loss_rate_history"].append(loss_rate)
                    
                    if len(node["loss_rate_history"]) > history_length:
                        node["loss_rate_history"].pop(0)
                        
                else:
                    print(f"Warning: No 'packetloss' field found in data from {node_id}")

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
    start_server = websockets.serve(handle_client, "0.0.0.0", 8765)
    loop.run_until_complete(start_server)
    print("WebSocket Server listening on ws://0.0.0.0:8765")
    loop.run_forever()

# --- Matplotlib Plotting ---
fig, ax = plt.subplots(figsize=(8, 5))
ax.set_title("Real-time Packet Loss (%)")
ax.set_xlabel(f"Time (last {history_length} packets)")
ax.set_ylabel("Packet Loss (%)")
ax.set_ylim(0, 100)
ax.grid(True, linestyle='--', alpha=0.7)

lines = {}

def update_plot(frame):
    for node_id, node in nodes_data.items():
        if node_id not in lines:
            line, = ax.plot([], [], label=node_id, linewidth=2, marker='.')
            lines[node_id] = line
            ax.legend(loc="upper left")
        
        y_data = node["loss_rate_history"]
        x_data = list(range(len(y_data)))
        lines[node_id].set_data(x_data, y_data)
        
        # Adjust x-axis dynamically
        ax.set_xlim(0, max(history_length, len(x_data)))
        
        # Auto-scale Y axis for better visibility if loss is small
        current_max_loss = max([max(n["loss_rate_history"]) if n["loss_rate_history"] else 0 for n in nodes_data.values()] + [5.0])
        ax.set_ylim(0, min(100, current_max_loss * 1.5))

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
