#include "order_book.hpp"

#include <stdexcept>
#include <algorithm>
#include <limits>

using namespace std;

bool OrderBook::empty() const {
    return bids_.empty() && asks_.empty();
}

SubmitResult OrderBook::submit(
    Side side,
    int64_t price,
    int64_t quantity
) {
    if (price <= 0) {
        throw std::invalid_argument("Price must be positive");
    }

    if (side != Side::Buy && side != Side::Sell) {
        throw std::invalid_argument("Side must be Buy or Sell");
    }

    if (quantity <= 0) {
        throw std::invalid_argument("Quantity must be positive");
    }

    if (next_order_id_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("Order IDs exhausted");
    }

    Order order{next_order_id_++, side, price, quantity};
    SubmitResult result{order.id, quantity, {}};

    if (side == Side::Buy) {
        while (
            order.quantity > 0 &&
            !asks_.empty() &&
            asks_.begin()->first <= order.price
        ) {
            auto level = asks_.begin();
            Order& resting = level->second.front();

            const int64_t traded = min(order.quantity, resting.quantity);
            result.trades.push_back(Trade{
                order.id,
                resting.id,
                resting.price,
                traded
            });

            order.quantity -= traded;
            resting.quantity -= traded;

            if (resting.quantity == 0) {
                level->second.pop_front();
            }

            if (level->second.empty()) {
                asks_.erase(level);
            }
        }
        if (order.quantity > 0) { 
            bids_[price].push_back(order);
        }
    } else {
        while (order.quantity > 0 &&
               !bids_.empty() &&
               bids_.begin()->first >= order.price) {
            auto level = bids_.begin();
            Order& resting = level->second.front();

            const int64_t traded =
                min(order.quantity, resting.quantity);

            result.trades.push_back(Trade{
                resting.id,
                order.id,
                resting.price,
                traded
            });

            order.quantity -= traded;
            resting.quantity -= traded;

            if (resting.quantity == 0) {
                level->second.pop_front();
            }

            if (level->second.empty()) {
                bids_.erase(level);
            }
        }

        if (order.quantity > 0) {
            asks_[order.price].push_back(order);
        }
    }

    result.remaining_quantity = order.quantity;
    return result;
}
