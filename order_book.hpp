#ifndef TGT_EXCHANGE_ORDER_BOOK_HPP
#define TGT_EXCHANGE_ORDER_BOOK_HPP

#include "order.hpp"

#include <cstdint>
#include <cstddef>
#include <map>
#include <deque>
#include <functional>
#include <vector>

struct Trade {
    std::uint64_t buy_order_id;
    std::uint64_t sell_order_id;
    std::int64_t price;
    std::int64_t quantity;
};

struct SubmitResult {
    std::uint64_t order_id;
    std::int64_t remaining_quantity;
    std::vector<Trade> trades;
};

struct PriceLevel {
    std::int64_t price;
    std::int64_t quantity;
};

struct BookSnapshot {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};


class OrderBook {
    public:
        bool empty() const;
        SubmitResult submit(Side side, std::int64_t price, std::int64_t quantity);
        bool cancel(std::uint64_t order_id);
        BookSnapshot snapshot(std::size_t depth = 5) const;

    private:

        // bids map
        std::map<
            std::int64_t,
            std::deque<Order>,
            std::greater<std::int64_t>
        > bids_;

        // asks map
        std::map<
            std::int64_t,
            std::deque<Order>,
            std::less<std::int64_t>
        > asks_;

        // next order id
        std::uint64_t next_order_id_{1};
};

#endif