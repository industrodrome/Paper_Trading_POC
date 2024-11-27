from fastapi import FastAPI, WebSocket, HTTPException
from pydantic import BaseModel
from typing import Optional
import uuid
import asyncio
from Orderbook import OrderBook
import random
import time
app = FastAPI()
order = OrderBook()
# Model for Limit Order
class LimitOrder(BaseModel):
    id: str
    type: str  # Buy or Sell
    price: float
    quantity: int
# Model for Market Order
class MarketOrder(BaseModel):
    order_type: str  # Buy or Sell
    quantity: int
# Endpoint to handle adding market orders
@app.post("/add_market_order/")
async def add_market_order(order_data: MarketOrder):
    """Endpoint to receive and process market orders."""
    await order.add_market_order(order_data.order_type, order_data.quantity)
    print(f"Received Market Order: {order_data}")
    return {"status": "Market order received", "order_data": order_data.dict()}
# Endpoint to handle adding limit orders
@app.post("/add_order/")
async def add_order(order_data: LimitOrder):
    """Endpoint to receive and process limit orders."""
    await order.add_limit_order(order_data.dict())
    print(f"Received Limit Order: {order_data}")
    return {"status": "Limit order received", "order_data": order_data.dict()}
@app.get("/order_book/")
async def get_order_book():
    """Endpoint to fetch the current state of the order book."""
    return order.display_order_book()
# WebSocket endpoint for tick data streaming
@app.websocket("/ws/tick-data/")
async def tick_data_stream(websocket: WebSocket):
    """
    WebSocket endpoint for streaming tick-by-tick data.
    Replace the simulated tick data with real data from an AWS S3 bucket.
    """
    await websocket.accept()
    try:
        while True:
            # Simulate tick data
            tick_data = {
                "price": round(random.uniform(100, 200), 2),
                "timestamp": time.time(),
                "volume": random.randint(1, 100)
            }
            
            # Replace the simulation with real tick data from AWS S3:
            # tick_data = await fetch_tick_data_from_s3()
            await websocket.send_json(tick_data)
            await asyncio.sleep(0.1)  # Simulate tick interval
    except Exception as e:
        print(f"WebSocket error: {e}")
    finally:
        await websocket.close()
# Function to fetch real tick data from AWS S3
async def fetch_tick_data_from_s3():
    """
    Fetch tick data from AWS S3.
    Implement the logic to retrieve data from your S3 bucket here.
    """
    # Example: Use boto3 to fetch data from S3
    # import boto3
    # s3 = boto3.client('s3')
    # bucket_name = "your-bucket-name"
    # key = "path/to/your/tick-data.json"
    # response = s3.get_object(Bucket=bucket_name, Key=key)
    # data = response['Body'].read().decode('utf-8')
    # return json.loads(data)
    pass
# Start the FastAPI app
if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
 

 