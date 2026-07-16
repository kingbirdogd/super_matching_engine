#pragma once

#include <cstdint>

namespace matching_engine {

enum class OutputType : std::uint8_t {
    TradeEvent = 2,
    OrderFullyFilled = 3,
    OrderPartiallyFilled = 4,
};

}  // namespace matching_engine
