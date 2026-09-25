#include <cassert>
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

    std::cout << first_id.order_id << '\n';
    std::cout << second_id.order_id << '\n';
    std::cout << book.empty() << '\n';

    try {
        book.submit(Side::Buy, 1025, 0);
    } catch (const std::invalid_argument& error) {
        std::cout << error.what() << '\n';
    }

    std::cout << book.submit(Side::Buy, 1025, 1).order_id << '\n';
    // An incoming buy partially fills a resting sell.
    {
        OrderBook test_book{};

        auto sell = test_book.submit(Side::Sell, 1020, 5);
        auto buy = test_book.submit(Side::Buy, 1025, 3);

        assert(buy.trades.size() == 1);
        assert(buy.trades[0].buy_order_id == buy.order_id);
        assert(buy.trades[0].sell_order_id == sell.order_id);
        assert(buy.trades[0].price == 1020);
        assert(buy.trades[0].quantity == 3);
        assert(buy.remaining_quantity == 0);

        auto second_buy = test_book.submit(Side::Buy, 1020, 2);

        assert(second_buy.trades.size() == 1);
        assert(second_buy.trades[0].quantity == 2);
        assert(second_buy.trades[0].sell_order_id == sell.order_id);
        assert(test_book.empty());
    }

    // An incoming sell executes at the resting buyer's price.
    {
        OrderBook test_book{};

        auto buy = test_book.submit(Side::Buy, 1030, 4);
        auto sell = test_book.submit(Side::Sell, 1020, 4);

        assert(sell.trades.size() == 1);
        assert(sell.trades[0].buy_order_id == buy.order_id);
        assert(sell.trades[0].sell_order_id == sell.order_id);
        assert(sell.trades[0].price == 1030);
        assert(sell.trades[0].quantity == 4);
        assert(sell.remaining_quantity == 0);
        assert(test_book.empty());
    }

    std::cout << "Matching checks passed\n";
}
