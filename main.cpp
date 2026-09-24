#include <iostream>
#include <stdexcept>

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

    OrderBook book{};
    std::cout << std::boolalpha << book.empty() << '\n';

    auto first_id = book.submit(Side::Buy, 1025, 10);
    auto second_id = book.submit(Side::Buy, 1025, 5);

    std::cout << first_id << '\n';
    std::cout << second_id << '\n';
    std::cout << book.empty() << '\n';

    try {
        book.submit(Side::Buy, 1025, 0);
    } catch (const std::invalid_argument& error) {
        std::cout << error.what() << '\n';
    }

    std::cout << book.submit(Side::Buy, 1025, 1) << '\n';
}