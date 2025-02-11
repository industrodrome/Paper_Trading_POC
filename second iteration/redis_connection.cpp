#include <iostream>
#include <sw/redis++/redis++.h>
#include <uuid/uuid.h>

using namespace sw::redis;
using namespace std;

// Function to generate a unique order ID
string generate_uuid() {
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
    return string(uuid_str);
}

int main() {
    try {
        // Connect to Redis
        Redis redis("tcp://localhost:11866");

        string order_type, side;
        int quantity;
        double price;

        cout << "Enter Order (Market/Limit) Buy/Sell Quantity Price: ";
        cin >> order_type >> side >> quantity >> price;

        string order_id = generate_uuid();

        // Store the order in Redis
        redis.hset(order_id, {
            {"order_type", order_type},
            {"side", side},
            {"quantity", to_string(quantity)},
            {"price", to_string(price)}
            });

        cout << "Order stored with ID: " << order_id << endl;
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}
