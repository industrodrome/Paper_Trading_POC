import heapq
import asyncio
import logging
import uuid
from typing import Literal, Dict, List, Optional
from pydantic import BaseModel, Field

# Configure logging
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")


class Order(BaseModel):
    id: str = Field(..., description="Unique order identifier.")
    type: Literal["buy", "sell"] = Field(..., description="Order type: 'buy' or 'sell'.")
    price: float = Field(..., gt=0, description="Order price, must be positive.")
    quantity: int = Field(..., gt=0, description="Order quantity, must be positive.")
    stop_loss: Optional[float] = Field(None, description="Stop-loss price for the order.")
    take_profit: Optional[float] = Field(None, description="Take-profit price for the order.")


class Trade(BaseModel):
    buy_order_id: str
    sell_order_id: str
    price: float
    quantity: int


class PnLManager:
    def __init__(self, initial_capital: float):
        self.capital = initial_capital
        self.realized_pnl = 0.0
        self.unrealized_pnl = 0.0

    def update_for_trade(self, trade: Trade, order_type: Literal["buy", "sell"]):
        """
        Update capital and realized PnL based on trade execution.
        """
        if order_type == "buy":
            self.capital -= trade.price * trade.quantity
            self.realized_pnl -= trade.price * trade.quantity
        elif order_type == "sell":
            self.capital += trade.price * trade.quantity
            self.realized_pnl += trade.price * trade.quantity

    def calculate_unrealized_pnl(self, buy_orders: List[Order], sell_orders: List[Order], market_price: float):
        """
        Calculate unrealized PnL for open positions.
        """
        self.unrealized_pnl = 0.0
        for order in buy_orders:
            self.unrealized_pnl += (market_price - order.price) * order.quantity
        for order in sell_orders:
            self.unrealized_pnl += (order.price - market_price) * order.quantity

    def summary(self):
        """
        Return the current capital and PnL summary.
        """
        return {
            "capital": self.capital,
            "realized_pnl": self.realized_pnl,
            "unrealized_pnl": self.unrealized_pnl,
        }


class OrderBook:
    def __init__(self):
        self._buy_orders = []  # Max-heap for buy orders (price negated for heapq compatibility)
        self._sell_orders = []  # Min-heap for sell orders
        self._order_map: Dict[str, Order] = {}  # Map of all active orders by ID
        self._lock = asyncio.Lock()  # Prevent concurrent modification
        self.trade_history: List[Trade] = []  # Store executed trades
        self.market_price = 100.0  # Initial market price
        self.pnl_manager = PnLManager(initial_capital=10000.0)

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
            remaining_quantity = quantity
            while remaining_quantity > 0:
                if order_type == "buy" and self._sell_orders:
                    sell_price, _, sell_order = heapq.heappop(self._sell_orders)
                    matched_quantity = min(remaining_quantity, sell_order.quantity)
                    self._execute_trade(None, sell_order, sell_price, matched_quantity)
                    remaining_quantity -= matched_quantity
                elif order_type == "sell" and self._buy_orders:
                    buy_price, _, buy_order = heapq.heappop(self._buy_orders)
                    matched_quantity = min(remaining_quantity, buy_order.quantity)
                    self._execute_trade(buy_order, None, -buy_price, matched_quantity)
                    remaining_quantity -= matched_quantity
                else:
                    break  # No more orders to match

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

    def _execute_trade(self, buy_order: Optional[Order], sell_order: Optional[Order], price: float, quantity: int):
        """Record and finalize a trade, update PnL, and adjust order quantities."""
        trade = Trade(
            buy_order_id=buy_order.id if buy_order else "market",
            sell_order_id=sell_order.id if sell_order else "market",
            price=price,
            quantity=quantity,
        )
        self.trade_history.append(trade)
        logging.info(f"Trade executed: {trade}")

        # Update PnL and capital
        if buy_order:
            buy_order.quantity -= quantity
            if buy_order.quantity == 0:
                heapq.heappop(self._buy_orders)
                self._order_map.pop(buy_order.id, None)
            self.pnl_manager.update_for_trade(trade, "buy")

        if sell_order:
            sell_order.quantity -= quantity
            if sell_order.quantity == 0:
                heapq.heappop(self._sell_orders)
                self._order_map.pop(sell_order.id, None)
            self.pnl_manager.update_for_trade(trade, "sell")


class ConditionalOrderManager:
    def __init__(self, order_book: OrderBook):
        self.order_book = order_book
        self.stop_loss_orders = []
        self.take_profit_orders = []

    def add_order(self, order: Order):
        """
        Add a stop-loss or take-profit order to the respective list.
        """
        if order.stop_loss:
            self.stop_loss_orders.append(order)
        if order.take_profit:
            self.take_profit_orders.append(order)

    async def monitor_market_price(self):
        """
        Monitor market price and trigger conditional orders.
        """
        while True:
            async with self.order_book._lock:
                market_price = self.order_book.market_price

                # Check stop-loss orders
                for order in self.stop_loss_orders[:]:
                    if market_price <= order.stop_loss:
                        logging.info(f"Stop-loss triggered for order {order.id} at price {market_price}")
                        self.order_book._execute_trade(
                            order if order.type == "buy" else None,
                            None if order.type == "buy" else order,
                            market_price,
                            order.quantity,
                        )
                        self.stop_loss_orders.remove(order)

                # Check take-profit orders
                for order in self.take_profit_orders[:]:
                    if market_price >= order.take_profit:
                        logging.info(f"Take-profit triggered for order {order.id} at price {market_price}")
                        self.order_book._execute_trade(
                            order if order.type == "buy" else None,
                            None if order.type == "buy" else order,
                            market_price,
                            order.quantity,
                        )
                        self.take_profit_orders.remove(order)

            await asyncio.sleep(1)  # Adjust monitoring frequency as needed
