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

bool OrderBook::cancel(std::uint64_t order_id) {
    for (auto level = bids_.begin(); level != bids_.end(); ++level) {

        // get the deque for each level
        auto& orders = level->second;

        // loop thru the deque
        for (auto order = orders.begin(); order != orders.end(); ++order) {
            if (order->id == order_id) {
                orders.erase(order);

                if (orders.empty()) {
                    bids_.erase(level);
                }

                return true;
            }
        }
    }

    for (auto level = asks_.begin(); level != asks_.end(); ++level) {
        auto& orders = level->second;

        for (auto order = orders.begin(); order != orders.end(); ++order) {
            if (order->id == order_id) {
                orders.erase(order);

                if (orders.empty()) {
                    asks_.erase(level);
                }

                return true;
            }
        }
    }

    return false;
}

BookSnapshot OrderBook::snapshot(std::size_t depth) const {
    // return obj
    BookSnapshot result;

    // loop through bids - price level, deque
    for (const auto& [price, orders] : bids_) {
        
        // optional depth param
        if (result.bids.size() >= depth) {
            break;
        }

        std::int64_t total = 0;
        for (const Order& order : orders) {
            if (order.quantity >
                std::numeric_limits<std::int64_t>::max() - total) {
                    // make sure quantity of orders in a price level doesnt exceed int 64
                throw std::overflow_error("Price-level quantity overflow");
            }
            total += order.quantity;
        }

        result.bids.push_back(PriceLevel{price, total});
    }

    for (const auto& [price, orders] : asks_) {
        if (result.asks.size() >= depth) {
            break;
        }

        std::int64_t total = 0;
        for (const Order& order : orders) {
            if (order.quantity >
                std::numeric_limits<std::int64_t>::max() - total) {
                throw std::overflow_error("Price-level quantity overflow");
            }
            total += order.quantity;
        }

        result.asks.push_back(PriceLevel{price, total});
    }

    return result;
}