#ifndef TGT_EXCHANGE_ORDER_HPP
#define TGT_EXCHANGE_ORDER_HPP

#include <cstdint>

enum class Side {
    Buy,
    Sell
};

struct Order {
    std::uint64_t id;
    Side side;
    std::int64_t price;
    std::int64_t quantity;
};

#endif