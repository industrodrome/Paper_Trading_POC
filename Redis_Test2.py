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
        self.initialize_capital()

    def initialize_capital(self):
        """Initialize capital if not already set."""
        if not self.db.exists("capital"):
            self.db.set("capital", 10000)  # Set initial capital to 10000
        print(f"Current capital: {self.db.get('capital')}")

    def update_capital(self, amount):
        """Update capital by adding or subtracting the specified amount."""
        capital = float(self.db.get("capital"))
        capital += amount
        self.db.set("capital", capital)
        print(f"Updated capital: {capital}")

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
        print(f"Order placed: {order}")
        self.match_order(order)

    def match_order(self, new_order):
        """Match the new order with existing orders in the book."""
        orders = self.db.lrange("orders", 0, -1)  # Get all orders from Redis

        for index, existing_order_data in enumerate(orders):
            existing_order = json.loads(existing_order_data)

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
                    self.db.rpush("trade_log", json.dumps(transaction))
                    print(f"Order matched: {transaction}")

                    # Update capital
                    if new_order["type"] == "buy":
                        self.update_capital(-matched_quantity * existing_order["price"])
                    else:
                        self.update_capital(matched_quantity * existing_order["price"])

                    # Update quantities and statuses
                    new_order["quantity"] -= matched_quantity
                    existing_order["quantity"] -= matched_quantity

                    if existing_order["quantity"] == 0:
                        existing_order["status"] = "closed"

                    self.db.lset("orders", index, json.dumps(existing_order))  # Update existing order in Redis

                    if new_order["quantity"] == 0:
                        new_order["status"] = "closed"
                        break  # Exit loop once the new order is fully matched

        # Save the updated new_order back to Redis if not fully matched
        if new_order["status"] == "open":
            self.db.rpush("orders", json.dumps(new_order))

    def get_trade_log(self):
        """Retrieve the trade log."""
        trade_log = self.db.lrange("trade_log", 0, -1)
        return [json.loads(entry) for entry in trade_log]

    def get_open_orders(self):
        """Retrieve all open buy and sell orders."""
        orders = self.db.lrange("orders", 0, -1)
        buy_orders = []
        sell_orders = []

        for order_data in orders:
            order = json.loads(order_data)
            if order["status"] == "open":
                if order["type"] == "buy":
                    buy_orders.append(order)
                elif order["type"] == "sell":
                    sell_orders.append(order)

        return {"buy_orders": buy_orders, "sell_orders": sell_orders}


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
            print("3. View Trade Log")
            print("4. View Capital")
            print("5. View Open Orders")
            print("6. Exit")

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
                trade_log = self.order_manager.get_trade_log()
                print("\nTrade Log:")
                for trade in trade_log:
                    print(trade)
            elif choice == "4":
                print(f"Current Capital: {self.order_manager.db.get('capital')}")
            elif choice == "5":
                open_orders = self.order_manager.get_open_orders()
                print("\nOpen Buy Orders:")
                for order in open_orders["buy_orders"]:
                    print(order)
                print("\nOpen Sell Orders:")
                for order in open_orders["sell_orders"]:
                    print(order)
            elif choice == "6":
                print("Exiting...")
                break
            else:
                print("Invalid choice. Please try again.")


# Main execution
if __name__ == "__main__":
    order_manager = OrderBookManager(db)
    order_placer = OrderPlacer(order_manager)
    asyncio.run(order_placer.run())
