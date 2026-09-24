#include "order_book.hpp"

#include <stdexcept>

using namespace std;

bool OrderBook::empty() const {
    return bids_.empty() && asks_.empty();
}

uint64_t OrderBook::submit(
    Side side,
    int64_t price,
    int64_t quantity
) {
    if (quantity <= 0) {
        throw std::invalid_argument("Quantity must be positive");
    }

    if (side != Side::Buy || side != Side::Sell) {
        throw std::invalid_argument("Side must be Buy or Sell");
    }

    if (quantity <= 0) {
        throw std::invalid_argument("Quantity must be positive");
    }

    Order order{next_order_id_, side, price, quantity};

    if (side == Side::Buy) {
        bids_[price].push_back(order);
    } else {
        asks_[price].push_back(order);
    }

    ++next_order_id_;
    return order.id;
}