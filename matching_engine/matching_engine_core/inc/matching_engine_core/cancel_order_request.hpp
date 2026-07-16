#pragma once

#include <cstdint>

namespace matching_engine {

struct CancelOrderRequest {
    std::uint64_t order_id{0};
};

}  // namespace matching_engine
