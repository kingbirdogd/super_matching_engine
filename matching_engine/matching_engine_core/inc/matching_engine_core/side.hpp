#pragma once

#include <cstdint>

namespace matching_engine {

enum class Side : std::uint8_t {
    Buy = 0,
    Sell = 1,
};

}  // namespace matching_engine
