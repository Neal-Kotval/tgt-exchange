#ifndef TGT_EXCHANGE_ORDER_BOOK_HPP
#define TGT_EXCHANGE_ORDER_BOOK_HPP

#include "order.hpp"

#include <cstdint>
#include <map>
#include <deque>
#include <functional>

class OrderBook {
    public:
        bool empty() const;
        std::uint64_t submit(Side side, std::int64_t price, std::int64_t quantity);

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