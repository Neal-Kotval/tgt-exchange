#include "order_book.hpp"

#include <iostream>
#include <stdexcept>

// Unlike assert(), these checks stay enabled in Release builds.
void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int main() {
    try {
        // Price priority, FIFO, partial fills, and crossing multiple orders.
        {
            OrderBook book;
            auto older = book.submit(Side::Sell, 1025, 5);
            auto cheaper = book.submit(Side::Sell, 1020, 3);
            auto newer = book.submit(Side::Sell, 1025, 4);
            auto buy = book.submit(Side::Buy, 1025, 10);

            check(buy.trades.size() == 3, "Expected three trades");
            check(buy.trades[0].sell_order_id == cheaper.order_id,
                  "Cheapest ask must trade first");
            check(buy.trades[1].sell_order_id == older.order_id,
                  "Oldest equal-price order must trade first");
            check(buy.trades[2].sell_order_id == newer.order_id,
                  "Newest equal-price order must trade last");
            check(buy.trades[0].buy_order_id == buy.order_id,
                  "Trade must identify incoming buyer");
            check(buy.trades[0].quantity == 3, "First fill quantity");
            check(buy.trades[1].quantity == 5, "Second fill quantity");
            check(buy.trades[2].quantity == 2, "Partial fill quantity");
            check(buy.trades[0].price == 1020, "Resting ask price");
            check(buy.remaining_quantity == 0, "Buy should be filled");

            auto snapshot = book.snapshot();
            check(snapshot.asks.size() == 1, "One ask level remains");
            check(snapshot.asks[0].quantity == 2, "Two units remain");
            check(book.cancel(newer.order_id), "Cancel partial remainder");
            check(book.empty(), "Book empty after cancellation");
            check(!book.cancel(newer.order_id), "Reject repeated cancellation");
            check(!book.cancel(older.order_id), "Cannot cancel filled order");
            check(!book.cancel(999), "Reject unknown cancellation");
        }

        // Incoming sells take highest bids first, then FIFO.
        {
            OrderBook book;
            auto lower = book.submit(Side::Buy, 1010, 2);
            auto older = book.submit(Side::Buy, 1020, 2);
            auto newer = book.submit(Side::Buy, 1020, 2);
            auto sell = book.submit(Side::Sell, 1000, 7);

            check(sell.trades.size() == 3, "Three bid matches");
            check(sell.trades[0].buy_order_id == older.order_id,
                  "Highest bid and oldest order first");
            check(sell.trades[1].buy_order_id == newer.order_id, "Bid FIFO");
            check(sell.trades[2].buy_order_id == lower.order_id, "Lower bid last");
            check(sell.trades[0].sell_order_id == sell.order_id,
                  "Trade must identify incoming seller");
            check(sell.trades[0].price == 1020, "Resting bid price");
            check(sell.remaining_quantity == 1, "Sell remainder");

            auto snapshot = book.snapshot();
            check(snapshot.bids.empty(), "All bids filled");
            check(snapshot.asks.size() == 1, "Sell remainder stored");
            check(snapshot.asks[0].quantity == 1, "One unit rests");
            auto final_buy = book.submit(Side::Buy, 1000, 1);
            check(final_buy.trades.size() == 1, "Remainder can trade");
            check(book.empty(), "Filled price levels removed");
        }

        // Cancelling from either side preserves the remaining FIFO order.
        for (Side side : {Side::Buy, Side::Sell}) {
            OrderBook book;
            auto first = book.submit(side, 1000, 1);
            auto middle = book.submit(side, 1000, 1);
            auto last = book.submit(side, 1000, 1);
            check(book.cancel(middle.order_id), "Cancel middle order");
            check(!book.cancel(middle.order_id), "Middle order already removed");

            Side opposite = side == Side::Buy ? Side::Sell : Side::Buy;
            auto match = book.submit(opposite, 1000, 2);
            check(match.trades.size() == 2, "Two orders remain");
            const auto& first_trade = match.trades[0];
            const auto& last_trade = match.trades[1];
            check((side == Side::Buy ? first_trade.buy_order_id
                                     : first_trade.sell_order_id) == first.order_id,
                  "First order keeps priority");
            check((side == Side::Buy ? last_trade.buy_order_id
                                     : last_trade.sell_order_id) == last.order_id,
                  "Last order remains second");
            check(book.empty(), "No empty price levels left behind");
        }

        // Noncrossing orders, aggregation, depth, and independent snapshots.
        {
            OrderBook book;
            for (int i = 0; i < 6; ++i) {
                book.submit(Side::Buy, 1000 - i, 2);
                book.submit(Side::Sell, 1100 + i, 3);
            }
            auto extra = book.submit(Side::Buy, 1000, 4);
            check(extra.trades.empty(), "Noncrossing order must wait");
            check(extra.remaining_quantity == 4, "Full quantity rests");

            auto snapshot = book.snapshot(5);
            check(snapshot.bids.size() == 5, "Five bid levels");
            check(snapshot.asks.size() == 5, "Five ask levels");
            check(snapshot.bids[0].quantity == 6, "Aggregate quantity");
            check(snapshot.bids[4].price == 996, "Descending bids");
            check(snapshot.asks[4].price == 1104, "Ascending asks");
            check(book.snapshot().bids.size() == 6, "Default returns all bids");
            check(book.snapshot().asks.size() == 6, "Default returns all asks");
            check(book.snapshot(10).asks.size() == 6, "Depth beyond book size");
            check(book.snapshot(0).bids.empty(), "Zero bid depth");
            check(book.snapshot(0).asks.empty(), "Zero ask depth");
            check(book.cancel(extra.order_id), "Cancel aggregated order");
            check(book.snapshot().bids[0].quantity == 2, "New snapshot reflects cancel");
            check(snapshot.bids[0].quantity == 6, "Old snapshot stays unchanged");
        }

        // Invalid input leaves the book and ID counter unchanged.
        {
            OrderBook book;
            auto expect_invalid = [&](Side side, std::int64_t price,
                                      std::int64_t quantity) {
                bool rejected = false;
                try {
                    book.submit(side, price, quantity);
                } catch (const std::invalid_argument&) {
                    rejected = true;
                }
                check(rejected, "Invalid order must be rejected");
                check(book.empty(), "Rejected order must not enter book");
            };
            expect_invalid(Side::Buy, 0, 1);
            expect_invalid(Side::Sell, -1, 1);
            expect_invalid(Side::Buy, 1000, 0);
            expect_invalid(Side::Buy, 1000, -1);
            expect_invalid(static_cast<Side>(9), 1000, 1);
            check(book.submit(Side::Buy, 1000, 1).order_id == 1,
                  "Invalid submissions must not consume IDs");
        }

        std::cout << "All core checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
