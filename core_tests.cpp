#include "order_book.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_price_time_priority() {
    OrderBook book;
    const auto older = book.submit(Side::Sell, 1025, 5);
    const auto cheaper = book.submit(Side::Sell, 1020, 3);
    const auto newer = book.submit(Side::Sell, 1025, 4);

    const auto buy = book.submit(Side::Buy, 1025, 10);

    expect(buy.trades.size() == 3, "Expected three trades");
    expect(buy.trades[0].sell_order_id == cheaper.order_id, "Cheapest ask should trade first");
    expect(buy.trades[1].sell_order_id == older.order_id, "Older order should win FIFO priority");
    expect(buy.trades[2].sell_order_id == newer.order_id, "Newer order should trade last");
}

void test_partial_fill() {
    OrderBook book;
    book.submit(Side::Sell, 1000, 10);

    const auto buy = book.submit(Side::Buy, 1000, 4);
    const auto snapshot = book.snapshot();

    expect(buy.trades.size() == 1, "Expected one trade");
    expect(buy.trades[0].quantity == 4, "Four units should trade");
    expect(buy.remaining_quantity == 0, "Buy should be fully filled");
    expect(snapshot.asks[0].quantity == 6, "Six sell units should remain");
}

void test_crossing_multiple_orders() {
    OrderBook book;
    const auto highest = book.submit(Side::Buy, 1020, 2);
    const auto middle = book.submit(Side::Buy, 1010, 2);
    const auto lowest = book.submit(Side::Buy, 1000, 2);

    const auto sell = book.submit(Side::Sell, 1000, 7);

    expect(sell.trades.size() == 3, "Sell should cross three bids");
    expect(sell.trades[0].buy_order_id == highest.order_id, "Highest bid should trade first");
    expect(sell.trades[1].buy_order_id == middle.order_id, "Middle bid should trade second");
    expect(sell.trades[2].buy_order_id == lowest.order_id, "Lowest bid should trade last");
    expect(sell.remaining_quantity == 1, "One sell unit should remain");
}

void test_cancellation() {
    OrderBook book;
    const auto first = book.submit(Side::Buy, 1000, 1);
    const auto middle = book.submit(Side::Buy, 1000, 1);
    const auto last = book.submit(Side::Buy, 1000, 1);

    expect(book.cancel(middle.order_id), "Cancellation should succeed");

    const auto sell = book.submit(Side::Sell, 1000, 2);
    expect(sell.trades[0].buy_order_id == first.order_id, "First order should keep priority");
    expect(sell.trades[1].buy_order_id == last.order_id, "Last order should trade second");
    expect(book.empty(), "Book should be empty");
}

void test_invalid_cancellation() {
    OrderBook book;
    const auto order = book.submit(Side::Sell, 1000, 1);

    expect(!book.cancel(999), "Unknown order should not be cancelled");
    expect(book.cancel(order.order_id), "Resting order should be cancelled");
    expect(!book.cancel(order.order_id), "Repeated cancellation should fail");
}

void test_book_snapshot() {
    OrderBook book;

    for (int offset = 0; offset < 6; ++offset) {
        book.submit(Side::Buy, 1000 - offset, 2);
        book.submit(Side::Sell, 1100 + offset, 3);
    }
    book.submit(Side::Buy, 1000, 4);

    const auto snapshot = book.snapshot(5);
    expect(snapshot.bids.size() == 5, "Expected five bid levels");
    expect(snapshot.asks.size() == 5, "Expected five ask levels");
    expect(snapshot.bids[0].price == 1000, "Best bid should come first");
    expect(snapshot.bids[0].quantity == 6, "Same-price quantities should be combined");
    expect(snapshot.asks[0].price == 1100, "Best ask should come first");
}

void run(const std::string& name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}

int main() {
    try {
        run("price priority and FIFO", test_price_time_priority);
        run("partial fills", test_partial_fill);
        run("crossing multiple orders", test_crossing_multiple_orders);
        run("cancellation", test_cancellation);
        run("invalid cancellation", test_invalid_cancellation);
        run("book snapshots", test_book_snapshot);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }
}
