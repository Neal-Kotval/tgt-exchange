#include <iostream>
#include "order.hpp"
#include "order_book.hpp"

using namespace std;

int main() {
    Order test{ 10, Side::Buy, 11, 12};
    cout << test.id << '\n';
    if (test.side == Side::Buy) {
        cout << "Buy\n";
    } else {
        cout << "Sell\n";
    }
    cout << test.price << '\n';
    cout << test.quantity << '\n';
}