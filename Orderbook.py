import heapq
import asyncio
import logging
import uuid
from typing import Literal, Dict, List
from pydantic import BaseModel, Field
# Configure logging
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
class Order(BaseModel):
    id: str = Field(..., description="Unique order identifier.")
    type: Literal["buy", "sell"] = Field(..., description="Order type: 'buy' or 'sell'.")
    price: float = Field(..., gt=0, description="Order price, must be positive.")
    quantity: int = Field(..., gt=0, description="Order quantity, must be positive.")
class Trade(BaseModel):
    buy_order_id: str
    sell_order_id: str
    price: float
    quantity: int
class OrderBook:
    def __init__(self):
        self._buy_orders = []  # Max-heap for buy orders (price negated for heapq compatibility)
        self._sell_orders = []  # Min-heap for sell orders
        self._order_map: Dict[str, Order] = {}  # Map of all active orders by ID
        self._lock = asyncio.Lock()  # Prevent concurrent modification
        self.trade_history: List[Trade] = []  # Store executed trades
        self.market_price = 100.0  # Initial market price
    async def add_limit_order(self, order_data: dict):
        """Add a limit order to the order book."""
        async with self._lock:
            order = Order(**order_data)
            if order.id in self._order_map:
                raise ValueError(f"Duplicate order ID: {order.id}")
            self._order_map[order.id] = order
            if order.type == "buy":
                heapq.heappush(self._buy_orders, (-order.price, len(self._buy_orders), order))
                logging.info(f"Added limit buy order: {order}")
            else:
                heapq.heappush(self._sell_orders, (order.price, len(self._sell_orders), order))
                logging.info(f"Added limit sell order: {order}")
            await self._match_limit_orders()
    async def add_market_order(self, order_type: Literal["buy", "sell"], quantity: int):
        """Execute a market order immediately."""
        async with self._lock:
            if order_type == "buy":
                while quantity > 0 and self._sell_orders:
                    sell_price, _, sell_order = heapq.heappop(self._sell_orders)
                    matched_quantity = min(quantity, sell_order.quantity)
                    self._execute_trade(None, sell_order, sell_price, matched_quantity)
                    quantity -= matched_quantity
            elif order_type == "sell":
                while quantity > 0 and self._buy_orders:
                    buy_price, _, buy_order = heapq.heappop(self._buy_orders)
                    matched_quantity = min(quantity, buy_order.quantity)
                    self._execute_trade(buy_order, None, -buy_price, matched_quantity)
                    quantity -= matched_quantity
            else:
                raise ValueError("Invalid market order type.")
    async def _match_limit_orders(self):
        """Match buy and sell limit orders."""
        while self._buy_orders and self._sell_orders:
            buy_price, _, buy_order = self._buy_orders[0]
            sell_price, _, sell_order = self._sell_orders[0]
            # Check if the top buy and sell orders can be matched
            if -buy_price < sell_price:
                break  # No match possible
            matched_quantity = min(buy_order.quantity, sell_order.quantity)
            self._execute_trade(buy_order, sell_order, sell_price, matched_quantity)
    def _execute_trade(self, buy_order: Order, sell_order: Order, price: float, quantity: int):
        """Record and finalize a trade."""
        trade = Trade(
            buy_order_id=buy_order.id if buy_order else "market",
            sell_order_id=sell_order.id if sell_order else "market",
            price=price,
            quantity=quantity,
        )
        self.trade_history.append(trade)
        logging.info(f"Trade executed: {trade}")
        if buy_order:
            buy_order.quantity -= quantity
            if buy_order.quantity == 0:
                heapq.heappop(self._buy_orders)
                self._order_map.pop(buy_order.id, None)
        if sell_order:
            sell_order.quantity -= quantity
            if sell_order.quantity == 0:
                heapq.heappop(self._sell_orders)
                self._order_map.pop(sell_order.id, None)
    async def update_market_price(self):
        """Simulate market price updates."""
        while True:
            try:
                self.market_price += round(asyncio.random.uniform(-0.5, 0.5), 2)
                self.market_price = max(1, self.market_price)  # Prevent price going below 1
                logging.info(f"Market Price Updated: {self.market_price:.2f}")
                await asyncio.sleep(1)
            except Exception as e:
                logging.error(f"Error updating market price: {e}")
    def display_order_book(self):
    #Display the current state of the order book.
        try:
            buy_orders = [
                {"id": order.id, "price": -price, "quantity": order.quantity}
                for price, _, order in sorted(self._buy_orders, reverse=True)
            ]
            sell_orders = [
                {"id": order.id, "price": price, "quantity": order.quantity}
                for price, _, order in sorted(self._sell_orders)
            ]
            trade_history = [
                {
                    "buy_order_id": trade.buy_order_id,
                    "sell_order_id": trade.sell_order_id,
                    "price": trade.price,
                    "quantity": trade.quantity,
                }
                for trade in self.trade_history
            ]
            return {
                "market_price": self.market_price,
                "buy_orders": buy_orders,
                "sell_orders": sell_orders,
                "trade_history": trade_history,
            }
        except Exception as e:
            logging.error(f"Error displaying order book: {e}")
            return {
                "error": f"Failed to fetch order book due to {e}"
            }