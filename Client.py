
import streamlit as st
import requests
import uuid
import pandas as pd
# Server URL (Make sure the server is running locally on port 8000)
SERVER_URL = "http://localhost:8000"
# Function to fetch the current state of the order book
def fetch_order_book():
    response = requests.get(f"{SERVER_URL}/order_book/")
    if response.status_code == 200:
        return response.json()
    else:
        st.error(f"Failed to fetch order book. Response: {response.text}")
        return None
# Function to cancel an order
def cancel_order(order_id):
    response = requests.delete(f"{SERVER_URL}/cancel_order/{order_id}/")
    if response.status_code == 200:
        st.success(f"Order {order_id} canceled successfully!")
    else:
        st.error(f"Failed to cancel order {order_id}. Response: {response.text}")
# Streamlit UI for order placement and management
def display_order_form():
    st.title("Order Placement and Management")
    # Order placement section
    st.header("Place an Order")
    order_type = st.radio("Select Order Type", ("Market Order", "Limit Order"))
    side = st.radio("Select Order Side", ("Buy", "Sell"))
    if order_type == "Market Order":
        order_quantity = st.number_input("Quantity", min_value=1, step=1)
        if st.button("Place Market Order"):
            order_data = {
                "order_type": side.lower(),  # Buy or Sell in lowercase for consistency
                "quantity": order_quantity
            }
            response = requests.post(f"{SERVER_URL}/add_market_order/", json=order_data)
            if response.status_code == 200:
                st.success(f"Market order placed successfully! Response: {response.json()}")
            else:
                st.error(f"Failed to place market order. Response: {response.text}")
    elif order_type == "Limit Order":
        order_price = st.number_input("Price", min_value=0.1, step=0.1)
        order_quantity = st.number_input("Quantity", min_value=1, step=1)
        if st.button("Place Limit Order"):
            order_data = {
                "id": str(uuid.uuid4()),  # Generate unique ID for the order
                "type": side.lower(),  # Buy or Sell in lowercase for consistency
                "price": order_price,
                "quantity": order_quantity
            }
            response = requests.post(f"{SERVER_URL}/add_order/", json=order_data)
            if response.status_code == 200:
                st.success(f"Limit order placed successfully! Response: {response.json()}")
            else:
                st.error(f"Failed to place limit order. Response: {response.text}")
    # Order book display section
    # Display the order book
st.header("Current Order Book")
order_book = fetch_order_book()
if order_book:
    st.write(f"**Current Market Price:** {order_book['market_price']:.2f}")
    # Display buy orders
    st.subheader("Buy Orders")
    buy_orders_df = pd.DataFrame(order_book["buy_orders"])
    if not buy_orders_df.empty:
        st.table(buy_orders_df)
    else:
        st.write("No buy orders available.")
    # Display sell orders
    st.subheader("Sell Orders")
    sell_orders_df = pd.DataFrame(order_book["sell_orders"])
    if not sell_orders_df.empty:
        st.table(sell_orders_df)
    else:
        st.write("No sell orders available.")
    # Display trade history
    st.subheader("Trade History")
    trade_history_df = pd.DataFrame(order_book["trade_history"])
    if not trade_history_df.empty:
        st.table(trade_history_df)
    else:
        st.write("No trades yet.")
# Run the Streamlit app
if __name__ == "__main__":
    display_order_form()