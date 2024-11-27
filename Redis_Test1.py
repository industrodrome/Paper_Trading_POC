#Tests connection to Redis 
#Test to set capital and update it
#And stores trade log

import os
import redis
import asyncio
import uuid
import json
from datetime import datetime

# Load Redis connection details from environment variables
REDIS_HOST = os.getenv("REDIS_HOST", "redis-14262.c305.ap-south-1-1.ec2.redns.redis-cloud.com")
REDIS_PORT = int(os.getenv("REDIS_PORT", 14262))
REDIS_USERNAME = os.getenv("REDIS_USERNAME", "default")
REDIS_PASSWORD = os.getenv("REDIS_PASSWORD", "lSvuqdS7WRkMbHqL9cjQ1abnkOHBdsuz")

# Initialize Redis client
try:
    db = redis.Redis(
        host=REDIS_HOST,
        port=REDIS_PORT,
        username=REDIS_USERNAME,
        password=REDIS_PASSWORD,
        db=0,
        decode_responses=True
    )
    if db.ping():
        print("Connected to Redis successfully!")
except redis.AuthenticationError as e:
    print(f"Authentication error: {e}")
    exit(1)
except redis.ConnectionError as e:
    print(f"Connection error: {e}")
    exit(1)


# Class for managing the order book and matching orders
class OrderBookManager:
    def __init__(self, redis_client):
        self.db = redis_client

    def place_order(self, order_type, price, quantity):
        """Place a buy or sell order and attempt to match it."""
        order = {
            "id": str(uuid.uuid4()),
            "type": order_type.lower(),
            "price": price,
            "quantity": quantity,
            "status": "open",
            "timestamp": datetime.now().isoformat()
        }
        self.db.rpush("orders", json.dumps(order))  # Store order in Redis
        print(f"Order placed: {order}")
        self.match_order(order)

    def match_order(self, new_order):
        """Match the new order with existing orders in the book."""
        orders = self.db.lrange("orders", 0, -1)  # Get all orders from Redis

        for index, existing_order in enumerate(orders):
            existing_order = json.loads(existing_order)
            if existing_order["status"] == "open" and existing_order["type"] != new_order["type"]:
                if (
                    (new_order["type"] == "buy" and new_order["price"] >= existing_order["price"]) or
                    (new_order["type"] == "sell" and new_order["price"] <= existing_order["price"])
                ):
                    # Match found
                    matched_quantity = min(new_order["quantity"], existing_order["quantity"])
                    transaction = {
                        "buy_order_id": new_order["id"] if new_order["type"] == "buy" else existing_order["id"],
                        "sell_order_id": new_order["id"] if new_order["type"] == "sell" else existing_order["id"],
                        "price": existing_order["price"],
                        "quantity": matched_quantity,
                        "timestamp": datetime.now().isoformat()
                    }

                    # Log the transaction in Redis
                    self.db.rpush("matched_orders", json.dumps(transaction))
                    print(f"Order matched: {transaction}")

                    # Update quantities and statuses
                    new_order["quantity"] -= matched_quantity
                    existing_order["quantity"] -= matched_quantity

                    if existing_order["quantity"] == 0:
                        existing_order["status"] = "closed"
                    if new_order["quantity"] == 0:
                        new_order["status"] = "closed"

                    # Update orders in Redis
                    self.db.lset("orders", index, json.dumps(existing_order))
                    break

        # Add the new order back to Redis if it's still open
        if new_order["status"] == "open":
            self.db.rpush("orders", json.dumps(new_order))

    def get_matched_orders(self):
        """Retrieve matched orders."""
        return self.db.lrange("matched_orders", 0, -1)


# Class for user interaction and order placement
class OrderPlacer:
    def __init__(self, order_manager):
        self.order_manager = order_manager

    async def run(self):
        """Run the order placer interface."""
        while True:
            print("\nOptions:")
            print("1. Place Buy Order")
            print("2. Place Sell Order")
            print("3. View Matched Orders")
            print("4. Exit")

            choice = input("Enter your choice: ")
            if choice == "1":
                price = float(input("Enter price: "))
                quantity = float(input("Enter quantity: "))
                self.order_manager.place_order("buy", price, quantity)
            elif choice == "2":
                price = float(input("Enter price: "))
                quantity = float(input("Enter quantity: "))
                self.order_manager.place_order("sell", price, quantity)
            elif choice == "3":
                matched_orders = self.order_manager.get_matched_orders()
                print("\nMatched Orders:")
                for order in matched_orders:
                    print(json.loads(order))
            elif choice == "4":
                print("Exiting...")
                break
            else:
                print("Invalid choice. Please try again.")


# Main execution
if __name__ == "__main__":
    order_manager = OrderBookManager(db)
    order_placer = OrderPlacer(order_manager)
    asyncio.run(order_placer.run())
