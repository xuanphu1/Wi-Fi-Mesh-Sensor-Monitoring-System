import asyncio
import websockets
import json
import random

async def simulate_node(node_id, loss_probability=0.05, interval=0.5):
    """
    Simulates an ESP32 node sending packets to the server.
    Randomly drops packets based on loss_probability to test the server's detection.
    """
    uri = "ws://localhost:8765"
    
    print(f"[{node_id}] Connecting to {uri}...")
    try:
        async with websockets.connect(uri) as websocket:
            print(f"[{node_id}] Connected!")
            seq_num = 1
            
            current_loss_rate = 0.0
            
            while True:
                # Randomly fluctuate the packet loss a bit around the loss_probability
                current_loss_rate = min(100.0, max(0.0, current_loss_rate + random.uniform(-2.0, 2.0)))
                # Bias towards the target loss_probability
                current_loss_rate += (loss_probability * 100 - current_loss_rate) * 0.1
                
                payload = {
                    "i": node_id,
                    "packetloss": round(current_loss_rate, 2),
                    "seq": seq_num
                }
                
                await websocket.send(json.dumps(payload))
                print(f"[{node_id}] -> Sent packetloss {payload['packetloss']}%")
                
                seq_num += 1
                await asyncio.sleep(interval)
                
    except ConnectionRefusedError:
        print(f"[{node_id}] Connection failed. Is the server running?")

async def main():
    print("Starting client simulators...")
    # Run two simulated nodes concurrently with different loss probabilities
    await asyncio.gather(
        simulate_node("Node_A_Stable", loss_probability=0.02, interval=0.5), # 2% loss
        simulate_node("Node_B_Unstable", loss_probability=0.15, interval=0.3) # 15% loss
    )

if __name__ == "__main__":
    asyncio.run(main())
