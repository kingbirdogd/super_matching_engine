#pragma once

#include <matching_engine_core/side.hpp>

#include <cstdint>

namespace matching_engine {

struct AddOrderRequest {
    std::uint64_t order_id{0};
    Side side{Side::Buy};
    std::uint64_t quantity{0};
    long double price{0.0L};
};

}  // namespace matching_engine
