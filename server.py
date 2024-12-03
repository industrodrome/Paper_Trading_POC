#Server 

from fastapi import FastAPI, HTTPException # type: ignore
import redis # type: ignore
import uuid
import json
from datetime import datetime
from pydantic import BaseModel # type: ignore
import logging
logging.basicConfig(level=logging.INFO)


# Redis connection details
REDIS_HOST = "redis-14262.c305.ap-south-1-1.ec2.redns.redis-cloud.com"
REDIS_PORT = 14262
REDIS_USERNAME = "default"
REDIS_PASSWORD = "lSvuqdS7WRkMbHqL9cjQ1abnkOHBdsuz"

# Initialize Redis client
db = redis.Redis(
    host=REDIS_HOST,
    port=REDIS_PORT,
    username=REDIS_USERNAME,
    password=REDIS_PASSWORD,
    db=0,
    decode_responses=True
)

app = FastAPI(title="Order Matching API", version="2.0")

# Pydantic models
class Order(BaseModel):
    username: str
    type: str
    price: float
    quantity: int

class CapitalUpdate(BaseModel):
    username: str
    capital: float

# Add or update capital for a user
@app.post("/update_capital/")
def update_capital(data: CapitalUpdate):
    if data.capital < 0:
        raise HTTPException(status_code=400, detail="Capital cannot be negative.")

    db.hset("user_capital", data.username, data.capital)
    return {"message": "Capital updated successfully", "username": data.username, "capital": data.capital}

# Get capital for a user
@app.get("/get_capital/{username}")
def get_capital(username: str):
    capital = db.hget("user_capital", username)
    if capital is None:
        raise HTTPException(status_code=404, detail="User not found or capital not set.")

    return {"username": username, "capital": float(capital)}

# Add an order to Redis sorted set
@app.post("/add_order/")
def add_order(order: Order):
    if order.type not in ["buy", "sell"]:
        raise HTTPException(status_code=400, detail="Order type must be 'buy' or 'sell'.")

    order_id = str(uuid.uuid4())
    order_data = {
        "id": order_id,
        "username": order.username,
        "type": order.type,
        "price": order.price,
        "quantity": order.quantity,
        "status": "open",
        "timestamp": str(datetime.now())
    }

    score = order.price if order.type == "sell" else -order.price
    key = "sell_orders" if order.type == "sell" else "buy_orders"
    db.hset(key, order_id, json.dumps(order_data))
    db.zadd(f"{key}_index", {order_id: score})

    match_orders()
    return {"message": "Order added successfully", "order_id": order_id}

# Match orders
def match_orders():
    while True:
        buy_orders = db.zrevrange("buy_orders_index", 0, 0)
        sell_orders = db.zrange("sell_orders_index", 0, 0)

        if not buy_orders or not sell_orders:
            break

        buy_order_id = buy_orders[0]
        sell_order_id = sell_orders[0]

        buy_order = json.loads(db.hget("buy_orders", buy_order_id))
        sell_order = json.loads(db.hget("sell_orders", sell_order_id))

        if buy_order["price"] >= sell_order["price"]:
            trade_quantity = min(buy_order["quantity"], sell_order["quantity"])
            trade_price = sell_order["price"]
            trade_value = trade_quantity * trade_price

            # Fetch and update buyer's capital
            buyer_capital = float(db.hget("user_capital", buy_order["username"]) or 0)
            if buyer_capital < trade_value:
                raise HTTPException(
                    status_code=400,
                    detail=f"Insufficient capital for user {buy_order['username']}."
                )
            db.hset("user_capital", buy_order["username"], buyer_capital - trade_value)

            # Fetch and update seller's capital
            seller_capital = float(db.hget("user_capital", sell_order["username"]) or 0)
            db.hset("user_capital", sell_order["username"], seller_capital + trade_value)

            # Create a trade record
            trade = {
                "buy_id": buy_order["id"],
                "sell_id": sell_order["id"],
                "buy_user": buy_order["username"],
                "sell_user": sell_order["username"],
                "price": trade_price,
                "quantity": trade_quantity,
                "timestamp": str(datetime.now())
            }

            db.rpush("trade_history", json.dumps(trade))

            # Update quantities
            buy_order["quantity"] -= trade_quantity
            sell_order["quantity"] -= trade_quantity

            # Update status for fully matched orders
            if buy_order["quantity"] == 0:
                buy_order["status"] = "closed"
                db.zrem("buy_orders_index", buy_order_id)
                db.hdel("buy_orders", buy_order_id)
            else:
                db.hset("buy_orders", buy_order_id, json.dumps(buy_order))

            if sell_order["quantity"] == 0:
                sell_order["status"] = "closed"
                db.zrem("sell_orders_index", sell_order_id)
                db.hdel("sell_orders", sell_order_id)
            else:
                db.hset("sell_orders", sell_order_id, json.dumps(sell_order))
        else:
            break


# Retrieve order book for all users
@app.get("/order_book/")
def get_order_book():
    buy_orders = [
        json.loads(db.hget("buy_orders", order_id))
        for order_id in db.zrevrange("buy_orders_index", 0, -1)
    ]
    sell_orders = [
        json.loads(db.hget("sell_orders", order_id))
        for order_id in db.zrange("sell_orders_index", 0, -1)
    ]
    return {"buy_orders": buy_orders, "sell_orders": sell_orders}

# Retrieve trade history for all users
@app.get("/trade_history/")
def get_trade_history():
    trades = [json.loads(trade) for trade in db.lrange("trade_history", 0, -1)]
    return {"trade_history": trades}

# Retrieve order book for a specific user
@app.get("/order_book/{username}")
def get_user_order_book(username: str):
    buy_orders = [
        json.loads(db.hget("buy_orders", order_id))
        for order_id in db.zrevrange("buy_orders_index", 0, -1)
        if json.loads(db.hget("buy_orders", order_id))["username"] == username
    ]
    sell_orders = [
        json.loads(db.hget("sell_orders", order_id))
        for order_id in db.zrange("sell_orders_index", 0, -1)
        if json.loads(db.hget("sell_orders", order_id))["username"] == username
    ]
    return {"buy_orders": buy_orders, "sell_orders": sell_orders}

# Retrieve trade history for a specific user
@app.get("/trade_history/{username}")
def get_user_trade_history(username: str):
    trades = [
        json.loads(trade) for trade in db.lrange("trade_history", 0, -1)
        if json.loads(trade)["buy_user"] == username or json.loads(trade)["sell_user"] == username
    ]
    return {"trade_history": trades}