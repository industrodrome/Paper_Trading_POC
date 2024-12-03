#Client
import requests # type: ignore

BASE_URL = "http://127.0.0.1:8000"

def update_capital(username, capital):
    payload = {"username": username, "capital": capital}
    response = requests.post(f"{BASE_URL}/update_capital/", json=payload)
    if response.status_code == 200:
        print(response.json()["message"])
    else:
        print("Failed to update capital:", response.json()["detail"])

def get_capital(username):
    response = requests.get(f"{BASE_URL}/get_capital/{username}")
    if response.status_code == 200:
        return response.json()["capital"]
    else:
        print("Failed to retrieve capital:", response.json()["detail"])
        return None

def place_order(username):
    order_type = input("Enter order type (buy/sell): ").strip().lower()
    price = float(input("Enter price: "))
    quantity = int(input("Enter quantity: "))
    payload = {"username": username, "type": order_type, "price": price, "quantity": quantity}
    response = requests.post(f"{BASE_URL}/add_order/", json=payload)
    if response.status_code == 200:
        print(response.json()["message"])
    else:
        print("Failed to place order:", response.json()["detail"])

    # Display updated capital after the trade
    updated_capital = get_capital(username)
    if updated_capital is not None:
        print(f"Updated Capital: {updated_capital}")

def view_order_book(username):
    response = requests.get(f"{BASE_URL}/order_book/{username}")
    if response.status_code == 200:
        order_book = response.json()
        print("\nYour Buy Orders:")
        for order in order_book["buy_orders"]:
            print(order)
        print("\nYour Sell Orders:")
        for order in order_book["sell_orders"]:
            print(order)
    else:
        print("Failed to retrieve order book:", response.json()["detail"])

def view_trade_history(username):
    response = requests.get(f"{BASE_URL}/trade_history/{username}")
    if response.status_code == 200:
        trade_history = response.json()
        print("\nYour Trade History:")
        for trade in trade_history["trade_history"]:
            print(trade)
    else:
        print("Failed to retrieve trade history:", response.json()["detail"])

def main():
    username = input("Enter your username: ").strip()
    capital = float(input("Enter your initial capital: "))
    update_capital(username, capital)

    while True:
        print("\nMenu:")
        print("1. Place an Order")
        print("2. View Your Order Book")
        print("3. View Your Trade History")
        print("4. Exit")
        choice = input("Enter your choice: ")
        if choice == "1":
            place_order(username)
        elif choice == "2":
            view_order_book(username)
        elif choice == "3":
            view_trade_history(username)
        elif choice == "4":
            break
        else:
            print("Invalid choice. Please try again.")

if __name__ == "__main__":
    main()
